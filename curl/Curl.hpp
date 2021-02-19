#pragma once
#include <curl/curl.h>
#include <curl/easy.h>
#include <string.h>

#include <cassert>
#include <functional>
#include <map>
#include <string>

#include <string/String.hpp>

#include <../unsorted/Raii.hpp>

namespace Curl {
    struct Multi;
    struct Client
    {
        friend struct Multi;
        using Error = std::runtime_error;

        using Headers     = std::map<std::string, std::string>;
        using RecvHandler = std::function<size_t(void*, size_t)>;

        enum class Method
        {
            GET,
            HEAD,
            POST,
            PUT,
            DELETE
        };

        struct Request
        {
            Method           method     = Method::GET;
            std::string      url        = {};
            std::string_view body       = {};
            std::string      username   = {};
            std::string      password   = {};
            std::string      cookie     = {};
            std::string      user_agent = {};
            Headers          headers    = {};

            time_t ims        = 0;
            time_t timeout_ms = 0;
            time_t connect_ms = 0;
            bool   verbose    = false;

            RecvHandler callback = {};
        };

        struct Result
        {
            int         status = 0;
            Headers     headers;
            std::string body;
            time_t      mtime = 0;

            void clear() { *this = {}; }
        };

        // default parameters
        struct Params
        {
            std::string username;
            std::string password;
            std::string cookie;
            std::string user_agent = "Curl++";
            Headers     headers;

            time_t timeout_ms = 3000;
            time_t connect_ms = 100;
            bool   verbose    = false;

            Params() {}
        };

        Client(const Params& aParams = Params())
        : m_Params(aParams)
        {
            m_Curl = curl_easy_init();
            if (m_Curl == nullptr) {
                throw Error("Curl: fail to create easy handle");
            }
        }
        ~Client()
        {
            detach();
            clear();
            curl_easy_cleanup(m_Curl);
        }

        static void GlobalInit()
        {
            int res = curl_global_init(CURL_GLOBAL_ALL);
            if (res != CURLE_OK) {
                throw Error("Curl: global init failed");
            }
        }

        Result& GET(const std::string& aUrl, time_t aIMS = 0)
        {
            return operator()(Request{.method = Method::GET, .url = aUrl, .ims = aIMS});
        }
        int GET(const std::string& aUrl, RecvHandler aHandler, time_t aIMS = 0)
        {
            return operator()(Request{.method = Method::GET, .url = aUrl, .ims = aIMS, .callback = aHandler}).status;
        }
        Result& POST(const std::string& aUrl, std::string_view aData)
        {
            return operator()(Request{.method = Method::POST, .url = aUrl, .body = aData});
        }
        Result& PUT(const std::string& aUrl, std::string_view aData)
        {
            return operator()(Request{.method = Method::PUT, .url = aUrl, .body = aData});
        }
        Result& DELETE(const std::string& aUrl)
        {
            return operator()(Request{.method = Method::DELETE, .url = aUrl});
        }
        int HEAD(const std::string& aUrl)
        {
            return operator()(Request{.method = Method::HEAD, .url = aUrl}).status;
        }

        Result& operator()(const Request& aRequest)
        {
            clear();
            switch (aRequest.method) {
            case Method::GET:
                if (aRequest.ims > 0) {
                    setopt(CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
                    setopt(CURLOPT_TIMEVALUE, aRequest.ims);
                }
                break;
            case Method::POST:
                setopt(CURLOPT_POST, 1);
                setopt(CURLOPT_POSTFIELDS, aRequest.body.data());
                setopt(CURLOPT_POSTFIELDSIZE, aRequest.body.size());
                setopt(CURLOPT_EXPECT_100_TIMEOUT_MS, 0);
                break;
            case Method::PUT:
                setopt(CURLOPT_UPLOAD, 1);
                setopt(CURLOPT_INFILESIZE_LARGE, aRequest.body.size());
                setopt(CURLOPT_READDATA, this);
                setopt(CURLOPT_READFUNCTION, put_handler);
                setopt(CURLOPT_EXPECT_100_TIMEOUT_MS, 0);
                m_UploadData = aRequest.body;
                break;
            case Method::DELETE: setopt(CURLOPT_CUSTOMREQUEST, "DELETE"); break;
            case Method::HEAD: setopt(CURLOPT_NOBODY, 1); break;
            }
            return query(aRequest);
        }

        void clear()
        {
            curl_easy_reset(m_Curl);
            curl_slist_free_all(m_HeaderList);
            m_HeaderList   = nullptr;
            m_UploadOffset = 0;
            m_UploadData   = {};
            m_Result.clear();
        }

    protected:
        const Params m_Params;
        CURL*        m_Curl;
        curl_slist*  m_HeaderList = nullptr;
        CURLM*       m_Multi      = nullptr;
        Result       m_Result;

        std::string_view m_UploadData   = {};
        size_t           m_UploadOffset = 0;

        static size_t stream_handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            RecvHandler* sHandler = reinterpret_cast<RecvHandler*>(aUser);
            return (*sHandler)(aPtr, aSize * aBlock);
        }

        static size_t buf_handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            std::string* sBuffer = reinterpret_cast<std::string*>(aUser);
            sBuffer->append((const char*)aPtr, aSize * aBlock);
            return aSize * aBlock;
        }

        static size_t header_handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            Headers*         sHeaders = reinterpret_cast<Headers*>(aUser);
            std::string_view sBuf((const char*)aPtr, aSize * aBlock);
            String::trim(sBuf);

            auto sPos = sBuf.find(':');
            if (sPos > 0 and sPos != std::string_view::npos and sPos + 2 < sBuf.size()) {
                std::string sName(sBuf.substr(0, sPos));
                std::string sValue(sBuf.substr(sPos + 2));

                sHeaders->operator[](sName) = sValue;
            }

            return aSize * aBlock;
        }

        // Returning 0 will signal end-of-file to the library and cause it to stop the current transfer.
        // Your function must return the actual number of bytes that it stored in the data area pointed at by the pointer buffer.
        static size_t put_handler(char* buffer, size_t size, size_t nitems, void* aUser)
        {
            Client* self = reinterpret_cast<Client*>(aUser);
            if (self->m_UploadOffset == self->m_UploadData.size())
                return 0;
            if (self->m_UploadOffset > self->m_UploadData.size())
                return CURL_READFUNC_ABORT;
            size_t sAvail = std::min(size * nitems, self->m_UploadData.size() - self->m_UploadOffset);
            memcpy(buffer, self->m_UploadData.data() + self->m_UploadOffset, sAvail);
            self->m_UploadOffset += sAvail;
            return sAvail;
        }

        int get_http_status()
        {
            long sRes = 0;
            curl_easy_getinfo(m_Curl, CURLINFO_RESPONSE_CODE, &sRes);
            return sRes;
        }

        time_t get_mtime()
        {
            long sTime = 0;
            curl_easy_getinfo(m_Curl, CURLINFO_FILETIME_T, &sTime);
            return sTime > 0 ? sTime : 0;
        }

#define _GENERATE_GET(X)                                 \
    auto x_##X(const Request& aRequest)                  \
    {                                                    \
        return aRequest.X > 0 ? aRequest.X : m_Params.X; \
    }
        _GENERATE_GET(timeout_ms)
        _GENERATE_GET(connect_ms)
        _GENERATE_GET(verbose)
#undef _GENERATE_GET

#define _GENERATE_GET(X)                                      \
    const auto& x_##X(const Request& aRequest)                \
    {                                                         \
        return !aRequest.X.empty() ? aRequest.X : m_Params.X; \
    }
        _GENERATE_GET(username)
        _GENERATE_GET(password)
        _GENERATE_GET(cookie)
        _GENERATE_GET(user_agent)
        _GENERATE_GET(headers)
#undef _GENERATE_GET

        //Result& query(const std::string& aUrl, bool aOwnBuffer = false)
        Result& query(const Request& aRequest)
        {
            setopt(CURLOPT_URL, aRequest.url.c_str());

            if (aRequest.callback) {
                setopt(CURLOPT_WRITEFUNCTION, stream_handler);
                setopt(CURLOPT_WRITEDATA, reinterpret_cast<const void*>(&aRequest.callback));
            } else {
                setopt(CURLOPT_WRITEFUNCTION, buf_handler);
                setopt(CURLOPT_WRITEDATA, reinterpret_cast<void*>(&m_Result.body));
            }

            setopt(CURLOPT_HEADERFUNCTION, header_handler);
            setopt(CURLOPT_HEADERDATA, reinterpret_cast<void*>(&m_Result.headers));
            setopt(CURLOPT_FILETIME, 1);
            setopt(CURLOPT_NOSIGNAL, 1);
            setopt(CURLOPT_FOLLOWLOCATION, 1);
            setopt(CURLOPT_TIMEOUT_MS, x_timeout_ms(aRequest));
            setopt(CURLOPT_CONNECTTIMEOUT_MS, x_connect_ms(aRequest));
            //setopt(CURLOPT_BUFFERSIZE, 1024*1024);

            // user agent
            if (auto sUseragent = x_user_agent(aRequest); !sUseragent.empty())
                setopt(CURLOPT_USERAGENT, sUseragent.c_str());

            // set headers
            for (auto& [sName, sValue] : x_headers(aRequest)) {
                std::string sLine = sName + ": " + sValue;
                m_HeaderList      = curl_slist_append(m_HeaderList, sLine.c_str()); // make a copy
            }
            setopt(CURLOPT_HTTPHEADER, m_HeaderList);

            // auth
            if (auto sUsername = x_username(aRequest); !sUsername.empty()) {
                auto sPassword = x_password(aRequest);
                setopt(CURLOPT_USERNAME, sUsername.c_str());
                setopt(CURLOPT_PASSWORD, sPassword.c_str());
            }

            // cookies // "name1=content1; name2=content2;"
            if (auto sCookie = x_cookie(aRequest); !sCookie.empty())
                setopt(CURLOPT_COOKIE, sCookie.c_str());

            if (x_verbose(aRequest))
                setopt(CURLOPT_VERBOSE, 1);

            if (m_Multi) {
                int res = curl_multi_add_handle(m_Multi, m_Curl);
                if (res != CURLE_OK)
                    throw Error("Curl: fail to start multi call");
                m_Result.status = 100;
                return m_Result;
            }

            auto rc = curl_easy_perform(m_Curl);

            if (rc != CURLE_OK)
                throw Error(curl_easy_strerror(rc));

            m_Result.status = get_http_status();
            m_Result.mtime  = get_mtime();
            return m_Result;
        }

        // set options
        template <typename... Ts>
        void setopt(CURLoption aOption, Ts... aVal)
        {
            curl_easy_setopt(m_Curl, aOption, aVal...);
        }

        // support multi handle, used from Multi
        void attach(CURLM* aMulti)
        {
            m_Multi = aMulti;
        }

        void detach()
        {
            if (m_Multi) {
                curl_multi_remove_handle(m_Multi, m_Curl);
                m_Multi = nullptr;
            }
        }

    }; // namespace Curl
} // namespace Curl
