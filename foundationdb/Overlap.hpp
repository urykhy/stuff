#include "Client.hpp"

#include <time/Meter.hpp>

namespace FDB {
    class Overlap : public boost::noncopyable
    {
        Client&        m_Client;
        const uint64_t m_TimeoutMs = 5000;
        int            m_Current   = 0;

        using Ptr = std::unique_ptr<Transaction>;
        struct Info
        {
            Ptr         transaction;
            Time::Meter meter;
            Future      future;
            bool        busy = false;
        };
        Info     m_Info[2];
        unsigned m_Reads = 0;

        static constexpr double   LAG          = 0.5;
        static constexpr double   MAX_DURATION = 4;
        static constexpr unsigned MAX_READS    = 100000;

    public:
        Overlap(Client& aClient)
        : m_Client(aClient)
        {
            m_Info[0].transaction = std::make_unique<Transaction>(m_Client, m_TimeoutMs);
            m_Info[0].busy        = true;
        }

        Future Get(std::string_view aKey)
        {
            auto& sCurrent = m_Info[m_Current];
            if ((sCurrent.meter.get().to_double() > MAX_DURATION - LAG or m_Reads > MAX_READS / 2) and !m_Info[!m_Current].busy) {
                auto& sNext       = m_Info[!m_Current];
                sNext.transaction = std::make_unique<Transaction>(m_Client, m_TimeoutMs);
                sNext.future      = std::move(sNext.transaction->GetVersionTimestamp());
                sNext.meter.reset();
                sNext.busy = true;
            }

            if (sCurrent.meter.get().to_double() > MAX_DURATION or m_Reads > MAX_READS) {
                sCurrent.busy = false; // delay m_Transactions[m_Current].reset();
                sCurrent.meter.reset();
                m_Current = !m_Current;
                m_Reads   = 0;
            }

            m_Reads++;
            return m_Info[m_Current].transaction->Get(aKey, true /* snapshot */);
        }
    };
} // namespace FDB
