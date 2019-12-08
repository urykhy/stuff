#pragma once
#include <atomic>
#include <future>
#include <map>
#include <mutex>

#include <Curl.hpp>
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
        };
        using Result = std::future<Client::Result>;
        using CB = std::function<void(Result&&)>;
        using EasyPtr = std::shared_ptr<Client>;

        struct Query
        {
            EasyPtr easy;
            CB callback;
        };

        // no timeout while request in queue
        struct Request {
            std::string url;
            CB callback;
        };

    private:
        std::atomic_bool m_Stopping{false};
        const Params& m_Params;
        CURLM* m_Handle = nullptr;

        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;
        std::map<CURL*, Query> m_Active;
        Threads::SafeQueue<Request> m_Waiting;

    public:
        Multi(const Params& aParams)
        : m_Params(aParams)
        , m_Handle(curl_multi_init())
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
            if (activeCount() <= m_Params.max_connections and m_Waiting.idle()) {
                startRequest(aUrl, std::move(aCallback));
            } else {
                m_Waiting.insert({aUrl, aCallback});
            }
        }

        void Start(Threads::Group& tg) {
            tg.start([this]() {
                int transfers = 0;
                int queue = 0;
                while (!m_Stopping)
                {
                    // naive: no epoll, no errors
                    curl_multi_wait(m_Handle, 0, 0, 100, 0);
                    curl_multi_perform(m_Handle, &transfers);
                    while (auto x = curl_multi_info_read(m_Handle, &queue)) {
                        if (x->msg == CURLMSG_DONE)
                            notify(x->easy_handle, x->data.result);
                    }
                }
            });
            tg.at_stop([this]() {
                m_Stopping = true;
            });
        }

    private:
        void notify(CURL* aEasy, CURLcode rc) {
            Query sQuery;
            {
                Lock lk(m_Mutex);
                auto sIt = m_Active.find(aEasy);
                if (sIt == m_Active.end())
                    return; // WTF
                sQuery = std::move(sIt->second);
                m_Active.erase(sIt);
            }

            std::promise<Client::Result> sPromise;
            if (rc != CURLE_OK) {
                sPromise.set_exception(std::make_exception_ptr(Error(curl_easy_strerror(rc))));
            } else {
                sPromise.set_value(Client::Result{sQuery.easy->get_http_code(), std::move(sQuery.easy->m_Buffer)});
            }
            sQuery.callback(sPromise.get_future());
            startWaiting();
        }

        void startRequest(const std::string& aUrl, CB&& aCallback) {
            auto sEasy = std::make_shared<Client>(m_Params);
            sEasy->assign(m_Handle);
            Query sQuery{sEasy, std::move(aCallback)};
            {
                Lock lk(m_Mutex);
                m_Active.emplace(sEasy->m_Curl, std::move(sQuery));
            }
            sEasy->GET(aUrl);
        }

        void startWaiting() {
            while (activeCount() < m_Params.max_connections and !m_Waiting.idle())
            {
                auto sNew = m_Waiting.try_get();
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
