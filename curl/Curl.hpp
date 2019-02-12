#pragma once
#include <curl/curl.h>
#include <curl/easy.h>

#include <cassert>
#include <functional>
#include <string>
#include <../unsorted/Raii.hpp>

namespace Curl
{
    struct Client
    {
        using Error = std::runtime_error;

        // http code, response
        using Result = std::pair<int, std::string>;

        struct Params
        {
            time_t timeout_ms = 3000;
            std::string user_agent = "Curl/Client";
            std::string username;
            std::string password;
            std::string cookie;

            struct Header
            {
                std::string name;
                std::string value;
            };
            std::list<Header> headers;
        };

    private:
        const Params& m_Params;
        CURL *m_Curl;
        std::string m_Buffer;

        const std::string* m_UploadData = nullptr;
        size_t m_UploadOffset =  0;

        typedef std::function<size_t (void*, size_t)> HandlerT;
        static size_t handler(void* aPtr, size_t aSize, size_t aBlock, void* aUser)
        {
            std::string* sBuffer = reinterpret_cast<std::string*>(aUser);
            sBuffer->append((const char*)aPtr, aSize*aBlock);
            return aSize*aBlock;
        }

        // Returning 0 will signal end-of-file to the library and cause it to stop the current transfer.
        // Your function must return the actual number of bytes that it stored in the data area pointed at by the pointer buffer.
        static size_t read_proc(char *buffer, size_t size, size_t nitems, void *aUser)
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

    public:
        Client(const Params& aParams) : m_Params(aParams) { m_Curl = curl_easy_init(); assert(m_Curl); }
        ~Client() { curl_easy_cleanup(m_Curl); }

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
            setopt(CURLOPT_READFUNCTION, &read_proc);
            m_UploadData = &aData;
            return query(aUrl);
        }
        Result POST(const std::string& aUrl, const std::string& aData)
        {
            clear();
            setopt(CURLOPT_POST, 1);
            setopt(CURLOPT_POSTFIELDS, aData.c_str());
            setopt(CURLOPT_POSTFIELDSIZE, aData.size());
            return query(aUrl);
        }
        Result GET(const std::string& aUrl, time_t aIMS = 0)
        {
            clear();
            if (aIMS > 0)
            {
                setopt(CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
                setopt(CURLOPT_TIMEVALUE, aIMS);
            }
            return query(aUrl);
        }

        time_t get_mtime()
        {
            long sTime = 0;
            curl_easy_getinfo(m_Curl, CURLINFO_FILETIME, &sTime);
            return sTime > 0 ? sTime : 0;
        }

    protected:
        void clear()
        {
            curl_easy_reset(m_Curl);
            m_UploadOffset = 0;
            m_UploadData = nullptr;
        }

        Result query(const std::string& aUrl)
        {
            m_Buffer.clear();
            setopt(CURLOPT_URL, aUrl.c_str());
            setopt(CURLOPT_WRITEFUNCTION, &handler);
            setopt(CURLOPT_WRITEDATA, reinterpret_cast<void*>(&m_Buffer));
            setopt(CURLOPT_FILETIME, 1);
            setopt(CURLOPT_NOSIGNAL, 1);
            setopt(CURLOPT_FOLLOWLOCATION, 1);
            setopt(CURLOPT_TIMEOUT_MS, m_Params.timeout_ms);
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
    };
}
