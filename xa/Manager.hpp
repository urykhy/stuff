#pragma once

#include "Store.hpp"

namespace XA {

    class Manager
    {
        Store m_Store;

    public:
        std::optional<Store::XData> get(const std::string& aKey)
        {
            return m_Store.get(aKey);
        }

        template <class T>
        Store::Result perform(const std::string& aName, const std::string& aKey, T&& aHandler)
        {
            const auto sStatus = m_Store.status(aName);
            if (sStatus == Txn::Status::COMMITED)
                return Store::Result::SUCCESS;

            TxnBody sBody;
            sBody.name = aName;

            const auto sData = m_Store.get(aKey);
            sBody.key        = aKey;
            if (sData.has_value())
                sBody.version = sData->version;
            sBody.value = aHandler(sData);
            if (!sData.has_value() and !sBody.value.empty())
                sBody.operation = TxnBody::Operation::INSERT;
            else if (sData.has_value() and !sBody.value.empty())
                sBody.operation = TxnBody::Operation::UPDATE;
            else if (sData.has_value() and sBody.value.empty())
                sBody.operation = TxnBody::Operation::DELETE;
            else
                // (!sData.has_value() and sBody.value.empty())
                return Store::Result::SUCCESS;

            auto sResult = m_Store.prepare(sBody);
            if (sResult != Store::Result::SUCCESS) {
                m_Store.rollback(aName);
                return sResult;
            }
            return m_Store.commit(aName);
        }
    };

} // namespace XA