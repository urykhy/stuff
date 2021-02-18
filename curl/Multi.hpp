#pragma once
#include <atomic>
#include <future>
#include <map>
#include <mutex>

#include <container/RequestQueue.hpp>
#include <threads/Group.hpp>
#include <threads/SafeQueue.hpp>

#include "Curl.hpp"

namespace Curl {
    // https://curl.haxx.se/libcurl/c/libcurl-multi.html
    // https://gist.github.com/clemensg/5248927 - libuv demo

    // quick and dirty implementation
    struct Multi
    {
        using Error = Client::Error;
        struct Params
        {
            unsigned max_connections  = 32;
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
            Client::Request request;
            EasyPtr         easy;
            PromisePtr      promise;
            CB              callback;

            Request() {}
            Request(const Client::Request& aRequest, CB&& aCallback)
            : request(aRequest)
            , promise(std::make_shared<Promise>())
            , callback(std::move(aCallback))
            {}

            Request(const Request&) = delete;
            Request(Request&&)      = default;
            Request& operator=(const Request&) = delete;
            Request& operator=(Request&&) = default;

            void set_exception(CURLcode rc)
            {
                promise->set_exception(std::make_exception_ptr(Error(curl_easy_strerror(rc))));
            }

            void set_value()
            {
                auto&& sResult = easy->m_Result;
                sResult.status = easy->get_http_status();
                sResult.mtime  = easy->get_mtime();
                promise->set_value(std::move(sResult));
            }
        };

        std::atomic_bool m_Stopping{false};
        const Params     m_Params;
        CURLM*           m_Handle = nullptr;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex                m_Mutex;
        std::map<CURL*, Request>          m_Active;  // current requests
        container::RequestQueue<Request>  m_Waiting; // pending requests + expiration
        Threads::SafeQueueThread<Request> m_Next;    // call handlers from this thread
    public:
        Multi(const Params& aParams)
        : m_Params(aParams)
        , m_Handle(curl_multi_init())
        , m_Waiting([this](auto& aRequest) { // in queue request timeout. called with mutex held
            aRequest.promise->set_exception(std::make_exception_ptr(Error("timeout in queue")));
            m_Next.insert(std::move(aRequest));
        })
        , m_Next([](Request& aRequest) {
            aRequest.callback(aRequest.promise->get_future());
        })
        {
            if (m_Handle == nullptr) {
                throw Error("Curl: fail to create multi handle");
            }
            curl_multi_setopt(m_Handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, m_Params.max_connections);
        }
        ~Multi()
        {
            curl_multi_cleanup(m_Handle);
        }

        void GET(const std::string& aURL, CB&& aCallback)
        {
            operator()(Client::Request{.url = aURL}, std::move(aCallback));
        }

        void operator()(const Client::Request& aReq, CB&& aCallback)
        {
            if (activeCount() < m_Params.max_connections and m_Waiting.empty()) {
                startRequest(Request(aReq, std::move(aCallback)));
            } else {
                m_Waiting.insert(Request(aReq, std::move(aCallback)), m_Params.queue_timeout_ms);
            }
        }

        void start(Threads::Group& tg)
        {
            tg.start([this]() {
                int transfers = 0;
                int queue     = 0;
                while (!m_Stopping) {
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
        void notify(CURL* aEasy, CURLcode rc)
        {
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

        void startRequest(Request&& aRequest)
        {
            auto sEasy = std::make_shared<Client>();
            sEasy->assign(m_Handle);
            aRequest.easy = sEasy;
            auto sRequest = std::move(aRequest.request);
            {
                Lock lk(m_Mutex);
                m_Active.emplace(sEasy->m_Curl, std::move(aRequest));
            }
            sEasy->operator()(sRequest);
        }

        void startWaiting()
        {
            while (activeCount() < m_Params.max_connections and !m_Waiting.empty()) {
                auto sNew = m_Waiting.get();
                if (sNew)
                    startRequest(std::move(*sNew));
            }
        }

        size_t activeCount() const
        {
            Lock lk(m_Mutex);
            return m_Active.size();
        }
    };
} // namespace Curl
