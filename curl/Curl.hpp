#pragma once
#include <curl/curl.h>
#include <curl/easy.h>
#include <string.h>

#include <cassert>
#include <functional>
#include <map>
#include <sstream>
#include <string>

#include <../unsorted/Raii.hpp>
#include <string/String.hpp>

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
            std::string      user_agent = "Curl++";
            Headers          headers    = {};

            time_t ims        = 0;
            time_t timeout_ms = 3000;
            time_t connect_ms = 100;
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
        struct Default
        {
            std::string username;
            std::string password;
            std::string cookie;
            std::string user_agent = "Curl++";
            Headers     headers;
            bool        verbose = false;

            Request wrap(Request&& aRequest) const
            {
                if (aRequest.username.empty() and !username.empty()) {
                    aRequest.username = username;
                    aRequest.password = password;
                }

                if (aRequest.cookie.empty() and !cookie.empty())
                    aRequest.cookie = cookie;

                if (aRequest.user_agent.empty() and !user_agent.empty())
                    aRequest.user_agent = user_agent;

                for (auto& [sHeader, sValue] : headers)
                    if (aRequest.headers.find(sHeader) == aRequest.headers.end())
                        aRequest.headers[sHeader] = sValue;

                if (verbose)
                    aRequest.verbose = verbose;

                return aRequest;
            }
        };

        Client()
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
            m_HeaderList = nullptr;
            m_UploadData = {};
            m_Result.clear();
        }

    protected:
        CURL*       m_Curl;
        curl_slist* m_HeaderList = nullptr;
        CURLM*      m_Multi      = nullptr;
        Result      m_Result;

        std::string_view m_UploadData = {};

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
            if (self->m_UploadData.empty())
                return 0;
            size_t sAvail = std::min(size * nitems, self->m_UploadData.size());
            memcpy(buffer, self->m_UploadData.data(), sAvail);
            self->m_UploadData.remove_prefix(sAvail);
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
            setopt(CURLOPT_TIMEOUT_MS, aRequest.timeout_ms);
            setopt(CURLOPT_CONNECTTIMEOUT_MS, aRequest.connect_ms);
            //setopt(CURLOPT_BUFFERSIZE, 1024*1024);

            // user agent
            if (!aRequest.user_agent.empty())
                setopt(CURLOPT_USERAGENT, aRequest.user_agent.c_str());

            // set headers
            for (auto& [sName, sValue] : aRequest.headers) {
                std::string sLine = sName + ": " + sValue;
                auto sTmp = curl_slist_append(m_HeaderList, sLine.c_str()); // make a copy
                if (sTmp == nullptr)
                    throw Error("Curl: fail to slist_append");
                m_HeaderList = sTmp;
            }
            setopt(CURLOPT_HTTPHEADER, m_HeaderList);

            // auth
            if (!aRequest.username.empty()) {
                setopt(CURLOPT_USERNAME, aRequest.username.c_str());
                setopt(CURLOPT_PASSWORD, aRequest.password.c_str());
            }

            // cookies // "name1=content1; name2=content2;"
            if (!aRequest.cookie.empty())
                setopt(CURLOPT_COOKIE, aRequest.cookie.c_str());

            if (aRequest.verbose)
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
            auto sResult = curl_easy_setopt(m_Curl, aOption, aVal...);
            if (sResult != CURLE_OK) {
                std::stringstream tmp;
                tmp << "Curl: fail to set option " << aOption << ": " << curl_easy_strerror(sResult);
                throw Error(tmp.str());
            }
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
