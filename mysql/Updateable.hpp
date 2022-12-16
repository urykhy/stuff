#pragma once

#include <mutex>
#include <optional>

#include "Client.hpp"

#include <cache/Expiration.hpp>
#include <cbor/cbor.hpp>
#include <file/Interface.hpp>

namespace MySQL {
    template <class Policy>
    class Updateable : public File::Dumpable
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        typename Policy::Timestamp m_Timestamp = 0;
        typename Policy::Container m_Data;

        using Key   = typename Policy::Container::key_type;
        using Value = typename Policy::Container::mapped_type;
        using Iter  = typename Policy::Container::iterator;

        static constexpr size_t      MISS_MAX_SIZE = 1000;
        static constexpr time_t      MISS_TTL      = 10;
        Cache::Expiration<Key, bool> m_MissMap;

        void fallback(Iter& aIter, const Key& aKey, MySQL::Connection& aConnection)
        {
            typename Policy::Pair sResponse;
            bool                  sFound = false;
            aConnection.Query(Policy::fallback(aKey));
            aConnection.Use([&sResponse, &sFound](const MySQL::Row& aRow) {
                sResponse = Policy::parse(aRow);
                sFound    = true;
            });

            if (sFound) {
                bool sUnused;
                std::tie(aIter, sUnused) = m_Data.emplace(std::move(sResponse));
            } else {
                m_MissMap.Put(aKey, true);
            }
        }

    public:
        Updateable()
        : m_MissMap(MISS_MAX_SIZE, MISS_TTL)
        {
        }

        void dump(File::IWriter* aWriter) override
        {
            Lock lk(m_Mutex);
            cbor::write(*aWriter, m_Timestamp, m_Data);
        }

        void restore(File::IExactReader* aReader) override
        {
            typename Policy::Timestamp sTimestamp;
            typename Policy::Container sData;
            cbor::read(*aReader, sTimestamp, sData);
            Lock lk(m_Mutex);
            m_Timestamp = sTimestamp;
            std::swap(m_Data, sData);
            lk.unlock();
        }

        void update(MySQL::Connection& aConnection)
        {
            typename Policy::Container sData;
            aConnection.Query(Policy::query(m_Timestamp));
            aConnection.Use([&sData](const MySQL::Row& aRow) { sData.emplace(Policy::parse(aRow)); });

            Lock lk(m_Mutex);
            m_Timestamp = Policy::merge(sData, m_Data);
            lk.unlock();
        }
        std::optional<Value> find(const Key& aKey, MySQL::Connection& aConnection)
        requires std::is_invocable_v<decltype(&Policy::fallback), Key>
        {
            Lock lk(m_Mutex);
            auto sIter = m_Data.find(aKey);
            if (sIter == m_Data.end() and m_MissMap.Get(aKey) == nullptr) {
                fallback(sIter, aKey, aConnection);
            }
            if (sIter == m_Data.end())
                return std::nullopt;
            return std::make_optional<Value>(Value{sIter->second});
        }
        std::optional<Value> find(const Key& aKey)
        {
            Lock lk(m_Mutex);
            auto sIter = m_Data.find(aKey);
            if (sIter == m_Data.end())
                return std::nullopt;
            return std::make_optional<Value>(Value{sIter->second});
        }
        size_t size() const
        {
            Lock lk(m_Mutex);
            return m_Data.size();
        }
        bool empty() const
        {
            Lock lk(m_Mutex);
            return m_Data.empty();
        }
    };
} // namespace MySQL
