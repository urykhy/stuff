#pragma once

#include "Client.hpp"

namespace asio_tnt::cache {
    struct Entry
    {
        std::string key;
        time_t      timestamp = 0;
        std::string value;

        void parse(MsgPack::imemstream& in)
        {
            using namespace MsgPack;
            uint32_t array_len = read_array_size(in);
            if (array_len != 3)
                throw std::invalid_argument("msgpack array size is not 3");
            read_string(in, key);
            read_uint(in, timestamp);
            read_string(in, value);
        }
        void serialize(MsgPack::omemstream& out) const
        {
            using namespace MsgPack;
            write_array_size(out, 3);
            write_string(out, key);
            write_uint(out, timestamp);
            write_string(out, value);
        }
    };

    struct Engine
    {
        using ClientPtr = std::shared_ptr<Client>;

        using Result  = std::optional<Entry>;
        using Promise = std::promise<Result>;
        using Future  = std::future<Result>;
        using Handler = std::function<void(Future&&)>;

    private:
        ClientPtr m_Client;

        auto makeCallback(Handler&& aHandler)
        {
            return [sHandler = std::move(aHandler)](asio_tnt::Future&& aResult) {
                Promise sPromise;
                try {
                    Result sResult;
                    if (asio_tnt::parse<Entry>(aResult, [&sResult](auto&& x) { sResult = std::move(x); }) > 1)
                        throw std::invalid_argument("got more than one response element");
                    sPromise.set_value(std::move(sResult));
                } catch (const std::exception& e) {
                    sPromise.set_exception(std::current_exception());
                }
                sHandler(sPromise.get_future());
            };
        }

    public:
        Engine(boost::asio::io_service& aLoop, const tcp::endpoint& aAddr, unsigned aSpace)
        : m_Client(std::make_shared<Client>(aLoop, aAddr, aSpace))
        {
            m_Client->start();
        }

        ~Engine()
        {
            m_Client->stop();
        }

        bool is_alive() const { return m_Client->is_alive(); }

        void Get(const std::string& aKey, Handler aHandler)
        {
            auto sRequest = m_Client->formatSelect(IndexSpec().set_limit(1), aKey);
            m_Client->call(sRequest, makeCallback(std::move(aHandler)));
        }

        void Set(const Entry& aData, Handler aHandler)
        {
            auto sRequest = m_Client->formatInsert(aData);
            m_Client->call(sRequest, makeCallback(std::move(aHandler)));
        }

        void Delete(const std::string& aKey, Handler aHandler)
        {
            auto sRequest = m_Client->formatDelete(IndexSpec{}, aKey);
            m_Client->call(sRequest, makeCallback(std::move(aHandler)));
        }

        template <class H>
        void Expire(time_t aMinimal, unsigned aLimit, H&& aHandler)
        {
            struct ExpireResult
            {
                uint32_t affected = 0;

                void parse(MsgPack::imemstream& in)
                {
                    MsgPack::read_uint(in, affected);
                }
            };
            auto sRequest = m_Client->formatCall("expire_cache", aMinimal, aLimit);
            m_Client->call(sRequest, [aHandler = std::move(aHandler)](auto&& aResult) {
                std::promise<uint32_t> sPromise;
                try {
                    asio_tnt::parse<ExpireResult>(aResult, [&sPromise](auto&& x) {
                        sPromise.set_value(x.affected);
                    });
                } catch (const std::exception& e) {
                    sPromise.set_exception(std::current_exception());
                }
                aHandler(sPromise.get_future());
            });
        }
    };
} // namespace asio_tnt::cache