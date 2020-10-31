#pragma once

#include <mutex>
#include <optional>

#include <cbor/encoder.hpp>
#include <cbor/decoder.hpp>
#include <file/Interface.hpp>

#include <Client.hpp>

namespace MySQL {
    template <class Policy>
    class Updateable : public File::Dumpable
    {
        using Lock = std::unique_lock<std::mutex>;
        mutable std::mutex m_Mutex;

        typename Policy::Container m_Data;
        using Key   = typename Policy::Container::key_type;
        using Value = typename Policy::Container::mapped_type;

    public:
        Updateable() {}

        void dump(File::IWriter* aWriter) override
        {
            Lock lk(m_Mutex);
            cbor::write_type_value(*aWriter, cbor::CBOR_MAP, m_Data.size());
            for (auto& x : m_Data) {
                cbor::write(*aWriter, x.first);
                cbor::write(*aWriter, x.second);
            }
        }

        void restore(File::IExactReader* aReader) override
        {
            typename Policy::Container sData;
            size_t sSize = cbor::get_uint(*aReader, cbor::ensure_type(*aReader, cbor::CBOR_MAP));
            for (size_t i = 0; i < sSize; i++) {
                Key   key;
                Value value;
                cbor::read(*aReader, key);
                cbor::read(*aReader, value);
                sData.emplace(std::move(key), std::move(value));
            }
            Lock lk(m_Mutex);
            std::swap(m_Data, sData);
            lk.unlock();
        }

        void update(MySQL::Connection& aConnection)
        {
            typename Policy::Container sData;
            aConnection.Query(Policy::query());
            aConnection.Use([&sData](const MySQL::Row& aRow) { Policy::parse(sData, aRow); });
            Lock lk(m_Mutex);
            std::swap(m_Data, sData);
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
