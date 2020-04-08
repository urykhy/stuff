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

        using Promise = std::promise<Client::Result>;
        using PromisePtr = std::shared_ptr<Promise>;

        using Result  = std::future<Client::Result>;
        using CB = std::function<void(Result&&)>;
        using EasyPtr = std::shared_ptr<Client>;

        struct Request {    // pending request
            std::string url;
            CB callback;
        };

        struct Active {     // running request + curl handle
            EasyPtr easy;
            CB callback;
        };

        struct Tail {       // completed request + result
            PromisePtr promise;
            CB callback;

            Tail() {}
            Tail(CB&& aCallback)
            : promise(std::make_shared<Promise>())
            , callback(std::move(aCallback))
            {}
        };

    private:
        std::atomic_bool m_Stopping{false};
        const Params& m_Params;
        CURLM* m_Handle = nullptr;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::map<CURL*, Active> m_Active;           // current requests
        container::RequestQueue<Request> m_Waiting; // pending requests + expiration
        Threads::SafeQueueThread<Tail> m_Next;      // call handlers from this thread

    public:
        Multi(const Params& aParams)
        : m_Params(aParams)
        , m_Handle(curl_multi_init())
        , m_Waiting([this](auto& aRequest)
        {   // in queue request timeout. called with mutex held
            Tail sTail(std::move(aRequest.callback));
            sTail.promise->set_exception(std::make_exception_ptr(Error("timeout in queue")));
            m_Next.insert(sTail);
        })
        , m_Next([](Tail& aTail)
        {
            aTail.callback(aTail.promise->get_future());
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
                startRequest(aUrl, std::move(aCallback));
            } else {
                m_Waiting.insert({aUrl, aCallback}, m_Params.queue_timeout_ms);
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
            Active sActive;
            {
                Lock lk(m_Mutex);
                auto sIt = m_Active.find(aEasy);
                if (sIt == m_Active.end())
                    return; // WTF
                sActive = std::move(sIt->second);
                m_Active.erase(sIt);
            }

            Tail sTail(std::move(sActive.callback));
            if (rc != CURLE_OK) {
                sTail.promise->set_exception(std::make_exception_ptr(Error(curl_easy_strerror(rc))));
            } else {
                sTail.promise->set_value(Client::Result{sActive.easy->get_http_code(), std::move(sActive.easy->m_Buffer)});
            }
            m_Next.insert(sTail);
            startWaiting();
        }

        void startRequest(const std::string& aUrl, CB&& aCallback) {
            auto sEasy = std::make_shared<Client>(m_Params);
            sEasy->assign(m_Handle);
            Active sActive{sEasy, std::move(aCallback)};
            {
                Lock lk(m_Mutex);
                m_Active.emplace(sEasy->m_Curl, std::move(sActive));
            }
            sEasy->GET(aUrl);
        }

        void startWaiting() {
            while (activeCount() < m_Params.max_connections and !m_Waiting.empty())
            {
                auto sNew = m_Waiting.get();
                if (sNew)
                    startRequest(sNew->url, std::move(sNew->callback));
            }
        }

        size_t activeCount() const {
            Lock lk(m_Mutex);
            return m_Active.size();
        }
    };
} // namespace Curl
