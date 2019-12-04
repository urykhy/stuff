#pragma once

#include "Client.hpp"

namespace tnt17::cache
{
    struct Entry
    {
        std::string key;
        time_t      timestamp = 0;
        std::string value;

        void parse(MsgPack::imemstream& in)
        {
            using namespace MsgPack;
            uint32_t array_len = read_array_size(in);
            assert (array_len == 3);
            read_string(in, key);
            read_uint(in, timestamp);
            read_string(in, value);
        }
        void serialize(MsgPack::omemstream& out) const
        {
            using namespace MsgPack;
            write_array_size(out, 3);
            write_string(out, key);
            write_uint(out,   timestamp);
            write_string(out, value);
        }

        template<class S>
        static void formatKey(S& aStream, const std::string& aKey)
        {
            MsgPack::write_string(aStream, aKey);
        }
    };

    struct Engine
    {
        using XClient    = Client<Entry>;
        using XClientPtr = std::shared_ptr<XClient>;

        using Result  = std::optional<Entry>;
        using Promise = std::promise<Result>;
        using Future  = std::future<Result>;
        using Handler = std::function<void(Future&&)>;

    private:
        XClientPtr m_Client;

        // convert vector<T> to optional<T>
        auto makeCallback(Handler&& aHandler)
        {
            return [sHandler = std::move(aHandler)](auto&& aResult)
            {
                Promise sPromise;
                try
                {
                    Result sResult;
                    const auto sTmp = aResult.get();
                    if (sTmp.size() > 0)
                        sResult = sTmp[0];
                    sPromise.set_value(sResult);
                }
                catch(const std::exception& e)
                {
                    sPromise.set_exception(std::current_exception());
                }
                sHandler(sPromise.get_future());
            };
        }

    public:
        Engine(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aSpace)
        : m_Client(std::make_shared<XClient>(aLoop, aAddr, aSpace))
        {
            m_Client->start();
        }

        ~Engine()
        {
            m_Client->stop();
        }

        bool is_alive() const { return m_Client->is_alive(); }

        // return false if cache unavailable
        // true - if connection established and call placed
        bool Get(const std::string& aKey, Handler aHandler)
        {
            auto sRequest = m_Client->formatSelect(IndexSpec().set_limit(1), aKey);
            return m_Client->call(sRequest, makeCallback(std::move(aHandler)));
        }

        bool Set(const Entry& aData, Handler aHandler)
        {
            auto sRequest = m_Client->formatInsert(aData);
            return m_Client->call(sRequest,makeCallback(std::move(aHandler)));
        }

        bool Delete(const std::string& aKey, Handler aHandler)
        {
            auto sRequest = m_Client->formatDelete(IndexSpec{}, aKey);
            return m_Client->call(sRequest,makeCallback(std::move(aHandler)));
        }
    };
} // namespace tnt17