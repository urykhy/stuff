#pragma once

#include <unordered_map>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <cbor/cbor.hpp>
#include <unsorted/Enum.hpp>
#include <unsorted/Log4cxx.hpp>

namespace XA {

    namespace {
        namespace mi = boost::multi_index;
    }

    struct TxnBody
    {
        enum class Operation
        {
            IDLE,
            INSERT,
            UPDATE,
            DELETE,
        };
        inline static const std::unordered_map<Operation, std::string_view> sOperationMap{
            {Operation::IDLE, "idle"},
            {Operation::DELETE, "delete"},
            {Operation::INSERT, "insert"},
            {Operation::UPDATE, "update"},
        };

        std::string name      = {};
        Operation   operation = Operation::IDLE;
        std::string key       = {};
        uint64_t    version   = {};
        std::string value     = {};

        auto as_tuple() const { return std::tie(name, operation, key, version, value); }
        bool operator!=(const TxnBody& aOther) const { return as_tuple() != aOther.as_tuple(); }

        void cbor_write(cbor::ostream& out) const
        {
            cbor::write(out, name, unsigned(operation), key, version, value);
        }
        void cbor_read(cbor::istream& in)
        {
            unsigned tmp = 0;
            cbor::read(in, name, tmp, key, version, value);
            operation = (Operation)tmp;
        }
    };

    struct Txn
    {
        enum Status
        {
            UNKNOWN,
            PREPARE,
            COMMITED
        };

        inline static const std::unordered_map<Status, std::string_view> sStatusMap{
            {Status::UNKNOWN, "unknown"},
            {Status::PREPARE, "prepare"},
            {Status::COMMITED, "commited"},
        };

        uint64_t serial    = 0;  // transaction id
        Status   status    = {}; // commited or prepared
        time_t   commit_ts = 0;  // 0 if not commited
        TxnBody  txn;            // transaction info (key, value, version)

        struct by_serial;
        struct by_commit_ts;

        struct by_data_key;
        const std::string& data_key() const { return txn.key; }

        struct by_name;
        const std::string& name() const { return txn.name; }

        void cbor_write(cbor::ostream& out) const
        {
            cbor::write(out, serial, unsigned(status), commit_ts, txn);
        }
        void cbor_read(cbor::istream& in)
        {
            unsigned tmp = 0;
            cbor::read(in, serial, tmp, commit_ts, txn);
            status = (Status)tmp;
        }
    };

    struct Store
    {
        enum class Result
        {
            SUCCESS,
            NOT_FOUND,
            CONFLICT,
            INCONSISTENT,
            INVALID_ARGUMENT,
        };

        inline static const std::unordered_map<Result, std::string_view> sResultMap{
            {Result::SUCCESS, "success"},
            {Result::NOT_FOUND, "not-found"},
            {Result::CONFLICT, "conflict"},
            {Result::INCONSISTENT, "inconsistent"},
            {Result::INVALID_ARGUMENT, "invalid-argument"},
        };

        struct XData
        {
            uint64_t    version = 0;
            std::string data;
            void        cbor_write(cbor::ostream& out) const { cbor::write(out, version, data); }
            void        cbor_read(cbor::istream& in) { cbor::read(in, version, data); }
        };

    private:
        using Changelog = boost::multi_index_container<
            Txn,
            mi::indexed_by<
                mi::ordered_non_unique<
                    mi::tag<Txn::by_serial>,
                    mi::member<Txn, uint64_t, &Txn::serial>>,
                mi::hashed_non_unique<
                    mi::tag<Txn::by_data_key>,
                    mi::composite_key<
                        Txn,
                        mi::member<Txn, Txn::Status, &Txn::status>,
                        mi::const_mem_fun<Txn, const std::string&, &Txn::data_key>>>,
                mi::ordered_unique<
                    mi::tag<Txn::by_name>,
                    mi::const_mem_fun<Txn, const std::string&, &Txn::name>>>>;
        Changelog m_Log;

        using Data = std::unordered_map<std::string, XData>;
        Data m_Data;

        Result validate(const TxnBody& aTxn) const
        {
            log4cxx::NDC ndc(aTxn.key);
            auto         sItem = m_Data.find(aTxn.key);
            switch (aTxn.operation) {
            case TxnBody::Operation::INSERT:
                if (sItem != m_Data.end()) {
                    WARN("key already exists");
                    return Result::CONFLICT;
                }
                if (aTxn.version != 0) {
                    WARN("version must be 0");
                    return Result::INVALID_ARGUMENT;
                }
                break;
            case TxnBody::Operation::UPDATE:
                if (sItem == m_Data.end()) {
                    WARN("not found");
                    return Result::NOT_FOUND;
                }
                if (sItem->second.version != aTxn.version) {
                    WARN("version in store: " << sItem->second.version << ", while expected " << aTxn.version);
                    return Result::CONFLICT;
                }
                break;
            case TxnBody::Operation::DELETE:
                if (sItem == m_Data.end()) {
                    WARN("not found");
                    return Result::NOT_FOUND;
                }
                if (sItem->second.version != aTxn.version) {
                    WARN("version in store: " << sItem->second.version << ", while expected " << aTxn.version);
                    return Result::CONFLICT;
                }
                if (!aTxn.value.empty()) {
                    WARN("value must be empty");
                    return Result::INVALID_ARGUMENT;
                }
                break;
            case TxnBody::Operation::IDLE:
                return Result::INVALID_ARGUMENT;
            }

            return Result::SUCCESS;
        }

#ifdef BOOST_TEST_MODULE
    public:
#endif

        uint64_t next_serial()
        {
            auto& sStore = mi::get<typename Txn::by_serial>(m_Log);
            if (sStore.empty())
                return 1;
            auto sIt = sStore.rbegin();
            return sIt->serial + 1;
        }

    public:
        std::optional<XData> get(const std::string& aKey)
        {
            auto sIt = m_Data.find(aKey);
            if (sIt == m_Data.end())
                return {};
            return sIt->second;
        }

        Result prepare(const TxnBody& aTxn)
        {
            log4cxx::NDC ndc(aTxn.name);
            {
                // check if this transaction-name already prepared (or even used)
                auto& sStore = mi::get<typename Txn::by_name>(m_Log);
                auto  sIt    = sStore.find(aTxn.name);
                if (sIt != sStore.end()) {
                    if (sIt->txn != aTxn) {
                        WARN("attempt to reuse transaction name");
                        return Result::CONFLICT;
                    }
                    if (sIt->status == Txn::COMMITED) {
                        INFO("already commited");
                        return Result::CONFLICT;
                    }
                    return Result::SUCCESS;
                }
            }
            {
                // ensure we have no running transaction for key
                auto& sStore        = mi::get<typename Txn::by_data_key>(m_Log);
                auto [sBegin, sEnd] = sStore.equal_range(std::make_tuple(Txn::Status::PREPARE, aTxn.key));
                if (sBegin != sEnd) {
                    WARN("we already have prepared transaction [" << sBegin->txn.name << "] for key " << aTxn.key);
                    return Result::CONFLICT;
                }
            }
            // ensure data was not updated, and sanity checks ok
            const Result sValidate = validate(aTxn);
            if (sValidate != Result::SUCCESS)
                return sValidate;
            // OK, prepare
            m_Log.insert(Txn{
                .serial = 0,
                .status = Txn::Status::PREPARE,
                .txn    = aTxn});
            DEBUG("prepare " << aTxn.operation);
            return Result::SUCCESS;
        }

        Result rollback(const std::string& aName)
        {
            log4cxx::NDC ndc(aName);
            auto&        sStore = mi::get<typename Txn::by_name>(m_Log);
            auto         sIt    = sStore.find(aName);
            if (sIt == sStore.end()) {
                WARN("not found");
                return Result::NOT_FOUND;
            }
            if (sIt->status == Txn::Status::COMMITED) {
                ERROR("already commited");
                return Result::CONFLICT;
            }
            sStore.erase(sIt);
            ERROR("rollback");
            return Result::SUCCESS;
        }

        Result commit(const std::string& aName)
        {
            log4cxx::NDC ndc(aName);
            auto&        sStore = mi::get<typename Txn::by_name>(m_Log);
            auto         sIt    = sStore.find(aName);
            if (sIt == sStore.end()) {
                WARN("not found");
                return Result::NOT_FOUND;
            }
            if (sIt->status == Txn::Status::COMMITED) {
                INFO("already commited");
                return Result::SUCCESS;
            }
            if (!sStore.modify(sIt, [this](auto& p) {
                    auto& sTxn = p.txn;
                    switch (sTxn.operation) {
                    case TxnBody::Operation::INSERT: m_Data[sTxn.key] = {sTxn.version + 1, sTxn.value}; break;
                    case TxnBody::Operation::UPDATE: m_Data[sTxn.key] = {sTxn.version + 1, sTxn.value}; break;
                    case TxnBody::Operation::DELETE: m_Data.erase(sTxn.key); break;
                    default: assert(0);
                    }
                    p.serial    = next_serial();
                    p.status    = Txn::Status::COMMITED;
                    p.commit_ts = time(nullptr);
                })) {
                WARN("internal error");
                return Result::INCONSISTENT;
            }
            INFO("commited with serial " << sIt->serial);
            return Result::SUCCESS;
        }

        Txn::Status status(const std::string& aName)
        {
            auto& sStore = mi::get<typename Txn::by_name>(m_Log);
            auto  sIt    = sStore.find(aName);
            if (sIt == sStore.end())
                return Txn::Status::UNKNOWN;
            return sIt->status;
        }

        std::vector<std::string> list(Txn::Status aStatus = Txn::PREPARE)
        {
            std::vector<std::string> sResult;
            for (auto& x : m_Log)
                if (x.status == aStatus)
                    sResult.push_back(x.txn.name);
            return sResult;
        }

        void trim(const std::string& aNamePrefix)
        {
            log4cxx::NDC ndc("trim");
            auto&        sStore = mi::get<typename Txn::by_name>(m_Log);
            auto         sIt    = sStore.upper_bound(aNamePrefix);
            std::string  sBound = aNamePrefix;
            sBound[sBound.size() - 1]++;
            auto sUntil = sStore.lower_bound(sBound);
            while (sIt != sUntil) {
                DEBUG("transaction " << sIt->name());
                sIt = sStore.erase(sIt);
            }
        }

        void trim_pending()
        {
            log4cxx::NDC ndc("trim_pending");
            auto&        sStore = mi::get<typename Txn::by_serial>(m_Log);
            auto         sRange = sStore.equal_range(0);
            while (sRange.first != sRange.second) {
                DEBUG("transaction " << sRange.first->name());
                sRange.first = sStore.erase(sRange.first);
            }
        }

        void clear()
        {
            m_Log.clear();
            m_Data.clear();
        }

        std::string backup() const
        {
            return cbor::to_string(m_Log, m_Data);
        }

        void restore(const std::string& aData)
        {
            cbor::from_string(aData, m_Log, m_Data);
        }
    };

    _DECLARE_ENUM_TO_STRING(TxnBody::Operation, TxnBody::sOperationMap)
    _DECLARE_ENUM_TO_STRING(Txn::Status, Txn::sStatusMap)
    _DECLARE_ENUM_TO_STRING(Store::Result, Store::sResultMap)

} // namespace XA