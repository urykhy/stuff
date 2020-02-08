#pragma once
#include <curl/curl.h>
#include <curl/easy.h>
#include <string.h>

#include <cassert>
#include <functional>
#include <list>
#include <string>
#include <../unsorted/Raii.hpp>

namespace Curl
{
    struct Multi;
    struct Client
    {
        friend struct Multi;
        using Error = std::runtime_error;

        // http code, response
        using Result = std::pair<int, std::string>;

        struct Params
        {
            time_t timeout_ms = 3000;
            time_t connect_ms = 250;
            std::string user_agent = "Curl/Client";
            std::string username;
            std::string password;
            std::string cookie;
            bool verbose=false;

            struct Header
            {
                std::string name;
                std::string value;
            };
            std::list<Header> headers;
        };

        using RecvHandler = std::function<size_t (void*, size_t)>;

        Client(const Params& aParams) : m_Params(aParams) {
            m_Curl = curl_easy_init();
            if (m_Curl == nullptr) {
                throw Error("Curl: fail to create easy handle");
            }
        }
        ~Client() {
            if (m_Multi)
                curl_multi_remove_handle(m_Multi, m_Curl);
            curl_easy_cleanup(m_Curl);
        }

        static void GlobalInit()
        {
            int res = curl_global_init(CURL_GLOBAL_ALL);
            if (res != CURLE_OK) {
                throw Error("Curl: global init failed");
            }
        }

        Result DELETE(const std::string& aUrl)
        {
            clear();
            setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
            return query(aUrl);
        }
        Result PUT(const std::string& aUrl, const std::string& aData)
        {
            clear();
            setopt(CURLOPT_UPLOAD, 1);
            setopt(CURLOPT_INFILESIZE_LARGE, aData.size());
            setopt(CURLOPT_READDATA, this);
            setopt(CURLOPT_READFUNCTION, &put_handler);
            setopt(CURLOPT_EXPECT_100_TIMEOUT_MS, 0);
            m_UploadData = &aData;
            return query(aUrl);
        }
        Result POST(const std::string& aUrl, const std::string& aData)
        {
            clear();
            setopt(CURLOPT_POST, 1);
            setopt(CURLOPT_POSTFIELDS, aData.c_str());
            setopt(CURLOPT_POSTFIELDSIZE, aData.size());
            setopt(CURLOPT_EXPECT_100_TIMEOUT_MS, 0);
            return query(aUrl);
        }
        Result GET(const std::string& aUrl, time_t aIMS = 0)
        {
            clear();
            set_ims(aIMS);
            return query(aUrl);
        }
        int GET(const std::string& aUrl, RecvHandler aHandler, time_t aIMS = 0)
        {
            clear();
            set_ims(aIMS);
            void* sUser = reinterpret_cast<void*>(&aHandler);
            setopt(CURLOPT_WRITEFUNCTION, stream_handler);
            setopt(CURLOPT_WRITEDATA, sUser);
            return query(aUrl, true).first;
        }

        time_t get_mtime()
        {
            long sTime = 0;
            curl_easy_getinfo(m_Curl, CURLINFO_FILETIME, &sTime);
            return sTime > 0 ? sTime : 0;
        }

    protected:
        const Params& m_Params;
        CURL *m_Curl;
        CURLM* m_Multi = nullptr;
        std::string m_Buffer;

        const std::string* m_UploadData = nullptr;
        size_t m_UploadOffset =  0;

        void set_ims(time_t aIMS) {
            if (aIMS > 0)
            {
                setopt(CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
                setopt(CURLOPT_TIMEVALUE, aIMS);
            }
        }

        static size_t stream_handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            RecvHandler* sHandler = reinterpret_cast<RecvHandler*>(aUser);
            return (*sHandler)(aPtr, aSize*aBlock);
        }

        static size_t buf_handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            std::string* sBuffer = reinterpret_cast<std::string*>(aUser);
            sBuffer->append((const char*)aPtr, aSize*aBlock);
            return aSize*aBlock;
        }

        // Returning 0 will signal end-of-file to the library and cause it to stop the current transfer.
        // Your function must return the actual number of bytes that it stored in the data area pointed at by the pointer buffer.
        static size_t put_handler(char *buffer, size_t size, size_t nitems, void *aUser)
        {
            Client* self= reinterpret_cast<Client*>(aUser);
            if (self->m_UploadOffset == self->m_UploadData->size())
                return 0;
            if (self->m_UploadOffset > self->m_UploadData->size())
                return CURL_READFUNC_ABORT;
            size_t sAvail = std::min(size * nitems, self->m_UploadData->size() - self->m_UploadOffset);
            memcpy(buffer, self->m_UploadData->c_str() + self->m_UploadOffset, sAvail);
            self->m_UploadOffset += sAvail;
            return sAvail;
        }

        int get_http_code()
        {
            long sRes = 0;
            curl_easy_getinfo(m_Curl, CURLINFO_RESPONSE_CODE, &sRes);
            return sRes;
        }

        void clear()
        {
            curl_easy_reset(m_Curl);
            m_UploadOffset = 0;
            m_UploadData = nullptr;
        }

        Result query(const std::string& aUrl, bool aOwnBuffer = false)
        {
            m_Buffer.clear();
            setopt(CURLOPT_URL, aUrl.c_str());
            if (!aOwnBuffer)
            {
                setopt(CURLOPT_WRITEFUNCTION, &buf_handler);
                setopt(CURLOPT_WRITEDATA, reinterpret_cast<void*>(&m_Buffer));
            }
            setopt(CURLOPT_FILETIME, 1);
            setopt(CURLOPT_NOSIGNAL, 1);
            setopt(CURLOPT_FOLLOWLOCATION, 1);
            setopt(CURLOPT_TIMEOUT_MS, m_Params.timeout_ms);
            setopt(CURLOPT_CONNECTTIMEOUT_MS, m_Params.connect_ms);
            //setopt(CURLOPT_BUFFERSIZE, 1024*1024);

            // user agent
            if (!m_Params.user_agent.empty())
                setopt(CURLOPT_USERAGENT, m_Params.user_agent.c_str());

            // set headers
            struct curl_slist *sList = NULL;
            Util::Raii sCleanup([&sList](){
                curl_slist_free_all(sList);
            });
            for (auto& x : m_Params.headers)
            {
                std::string sLine = x.name + ": " + x.value;
                sList = curl_slist_append(sList, sLine.c_str());    // copies the string
            }
            setopt(CURLOPT_HTTPHEADER, sList);

            // auth
            if (!m_Params.username.empty())
            {
                setopt(CURLOPT_USERNAME, m_Params.username.c_str());
                setopt(CURLOPT_PASSWORD, m_Params.password.c_str());
            }

            // cookies // "name1=content1; name2=content2;"
            if (!m_Params.cookie.empty())
                setopt(CURLOPT_COOKIE, m_Params.cookie.c_str());

            if (m_Params.verbose)
                setopt(CURLOPT_VERBOSE, 1);

            if (m_Multi)
            {
                int res = curl_multi_add_handle(m_Multi, m_Curl);
                if (res != CURLE_OK)
                    throw Error("Curl: fail to start multi call");
                return Result(100, "");
            }

            auto rc = curl_easy_perform(m_Curl);

            if (rc != CURLE_OK)
                throw Error(curl_easy_strerror(rc));

            return Result(get_http_code(), std::move(m_Buffer));
        }

        ssize_t get_content_length()
        {
            double sFilesize = 0.0;
            int res = curl_easy_getinfo(m_Curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &sFilesize);
            if (CURLE_OK == res && sFilesize > 0.0)
                return sFilesize;
            return -1;
        }

        // set options
        template <typename ... Ts>
        void setopt(CURLoption aOption, Ts ... aVal)
        {
            curl_easy_setopt(m_Curl, aOption, aVal...);
        }

        // support multi handle, used from Multi
        void assign(CURLM* aMulti)
        {
            m_Multi = aMulti;
        }
    };
}
