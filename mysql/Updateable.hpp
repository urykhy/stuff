#pragma once

#include <mutex>
#include <optional>

#include "Client.hpp"

#include <cbor/cbor.hpp>
#include <file/Interface.hpp>

namespace MySQL {
    template <class Policy>
    class Updateable : public File::Dumpable
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        typename Policy::Container m_Data;
        typename Policy::Timestamp m_Timestamp = 0;

        using Key   = typename Policy::Container::key_type;
        using Value = typename Policy::Container::mapped_type;

    public:
        Updateable() {}

        void dump(File::IWriter* aWriter) override
        {
            Lock lk(m_Mutex);
            cbor::write(*aWriter, m_Data);
        }

        void restore(File::IExactReader* aReader) override
        {
            typename Policy::Container sData;
            cbor::read(*aReader, sData);
            Lock lk(m_Mutex);
            std::swap(m_Data, sData);
            lk.unlock();
        }

        void update(MySQL::Connection& aConnection)
        {
            typename Policy::Container sData;
            aConnection.Query(Policy::query(m_Timestamp));
            aConnection.Use([&sData](const MySQL::Row& aRow) { Policy::parse(sData, aRow); });

            Lock lk(m_Mutex);
            m_Timestamp = Policy::merge(sData, m_Data);
            lk.unlock();
        }
        std::optional<Value> find(const Key& k) const
        {
            Lock lk(m_Mutex);
            auto sIter = m_Data.find(k);
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
