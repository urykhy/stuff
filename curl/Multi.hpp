#pragma once
#include <atomic>
#include <future>
#include <map>
#include <mutex>

#include "Curl.hpp"
#include <container/RequestQueue.hpp>
#include <threads/Group.hpp>
#include <threads/SafeQueue.hpp>

namespace Curl
{
    // https://curl.haxx.se/libcurl/c/libcurl-multi.html
    // https://gist.github.com/clemensg/5248927 - libuv demo

    // quick and dirty implementation
    struct Multi
    {
        using Error = Client::Error;
        struct Params : Client::Params
        {
            unsigned max_connections = 32;
            unsigned queue_timeout_ms = 1000;
        };

        using Promise    = std::promise<Client::Result>;
        using PromisePtr = std::shared_ptr<Promise>;
        using Result     = std::future<Client::Result>;
        using CB         = std::function<void(Result&&)>;
        using EasyPtr    = std::shared_ptr<Client>;
    private:

        struct Request
        {
            std::string url;
            EasyPtr     easy;
            PromisePtr  promise;
            CB          callback;

            Request() {}
            Request(const std::string& aUrl, CB&& aCallback)
            : url(aUrl)
            , promise(std::make_shared<Promise>())
            , callback(std::move(aCallback))
            {}
            Request(Request&&) = default;
            Request(const Request&) = delete;
            Request& operator=(Request&&) = default;
            Request& operator=(Request&) = delete;

            void set_exception(CURLcode rc)
            {
                promise->set_exception(std::make_exception_ptr(Error(curl_easy_strerror(rc))));
            }

            void set_value()
            {
                promise->set_value(Client::Result{easy->get_http_code(), std::move(easy->m_Buffer)});
            }
        };

        std::atomic_bool m_Stopping{false};
        const Params& m_Params;
        CURLM* m_Handle = nullptr;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::map<CURL*, Request> m_Active;          // current requests
        container::RequestQueue<Request> m_Waiting; // pending requests + expiration
        Threads::SafeQueueThread<Request> m_Next;   // call handlers from this thread
    public:

        Multi(const Params& aParams)
        : m_Params(aParams)
        , m_Handle(curl_multi_init())
        , m_Waiting([this](auto& aRequest)
        {   // in queue request timeout. called with mutex held
            aRequest.promise->set_exception(std::make_exception_ptr(Error("timeout in queue")));
            m_Next.insert(std::move(aRequest));
        })
        , m_Next([](Request& aRequest)
        {
            aRequest.callback(aRequest.promise->get_future());
        })
        {
            if (m_Handle == nullptr) {
                throw Error("Curl: fail to create multi handle");
            }
            curl_multi_setopt(m_Handle, CURLMOPT_MAX_TOTAL_CONNECTIONS , m_Params.max_connections);
            curl_multi_setopt(m_Handle, CURLMOPT_PIPELINING, CURLPIPE_HTTP1);
        }
        ~Multi() {
            curl_multi_cleanup(m_Handle);
        }

        void GET(const std::string& aUrl, CB&& aCallback) {
            if (activeCount() < m_Params.max_connections and m_Waiting.empty()) {
                startRequest(Request(aUrl, std::move(aCallback)));
            } else {
                m_Waiting.insert(Request(aUrl, std::move(aCallback)), m_Params.queue_timeout_ms);
            }
        }

        void start(Threads::Group& tg) {
            tg.start([this]() {
                int transfers = 0;
                int queue = 0;
                while (!m_Stopping)
                {
                    const uint64_t sTimeout = m_Waiting.eta(100);
                    // naive: no epoll, no errors
                    curl_multi_wait(m_Handle, 0, 0, sTimeout, 0);
                    curl_multi_perform(m_Handle, &transfers);
                    while (auto x = curl_multi_info_read(m_Handle, &queue)) {
                        if (x->msg == CURLMSG_DONE)
                            notify(x->easy_handle, x->data.result);
                    }
                    m_Waiting.on_timer();
                }
            });
            tg.at_stop([this]() {
                m_Stopping = true;
            });
            m_Next.start(tg);
        }
    private:

        void notify(CURL* aEasy, CURLcode rc) {
            Request sRequest;
            {
                Lock lk(m_Mutex);
                auto sIt = m_Active.find(aEasy);
                if (sIt == m_Active.end())
                    return; // WTF
                sRequest = std::move(sIt->second);
                m_Active.erase(sIt);
            }

            if (rc != CURLE_OK) {
                sRequest.set_exception(rc);
            } else {
                sRequest.set_value();
            }
            sRequest.easy.reset();
            m_Next.insert(std::move(sRequest));
            startWaiting();
        }

        void startRequest(Request&& aRequest) {
            const std::string sUrl = aRequest.url;
            auto sEasy = std::make_shared<Client>(m_Params);
            sEasy->assign(m_Handle);
            aRequest.easy = sEasy;
            {
                Lock lk(m_Mutex);
                m_Active.emplace(sEasy->m_Curl, std::move(aRequest));
            }
            sEasy->GET(sUrl);
        }

        void startWaiting() {
            while (activeCount() < m_Params.max_connections and !m_Waiting.empty())
            {
                auto sNew = m_Waiting.get();
                if (sNew)
                    startRequest(std::move(*sNew));
            }
        }

        size_t activeCount() const {
            Lock lk(m_Mutex);
            return m_Active.size();
        }
    };
} // namespace Curl
