#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#define FAKEIT_ASSERT_ON_UNEXPECTED_METHOD_INVOCATION
#include <mock/fakeit.hpp>
#pragma GCC diagnostic pop

#include <variant>

#include "Client.hpp"

#include <mpl/Mpl.hpp>
#include <unsorted/Raii.hpp>

namespace MySQL {
    struct Mock
    {
        using Row  = std::vector<const char*>;
        using Rows = struct
        {
            std::vector<Row>              rows;
            std::vector<std::string_view> meta;
        };
        using Result = std::variant<Rows, Error>;

        struct Case
        {
            std::string query;
            Result      result = Rows{};
        };
        using SqlSet = std::vector<Case>;

    private:
        fakeit::Mock<ConnectionFace> m_Mock;
        const SqlSet&                m_SQL;
        size_t                       m_Serial    = 0;
        bool                         m_VerifyUse = false;

        void verifyUse()
        {
            using namespace fakeit;
            if (m_VerifyUse) {
                m_VerifyUse = false;
                Verify(Method(m_Mock, Use));
            }
        }

    public:
        Mock(const SqlSet& aSQL)
        : m_SQL(aSQL)
        {
            using namespace fakeit;

            When(Method(m_Mock, Query)).AlwaysDo([this](const std::string& aQuery) mutable {
                verifyUse();

                if (m_SQL[m_Serial].query == aQuery)
                    BOOST_CHECK_MESSAGE(true, "MOCK: " + m_SQL[m_Serial].query);
                else
                    BOOST_REQUIRE_MESSAGE(false, "MOCK: Unexpected query: " << aQuery << ", while waiting for: " << m_SQL[m_Serial].query);

                Util::Raii sBumpSerial([this]() { m_Serial++; });

                std::visit(Mpl::overloaded{
                               [this](const Rows& aRows) {
                                   if (!aRows.rows.empty()) {
                                       When(Method(m_Mock, Use)).Do([this, &aRows](auto&& aHandler) mutable {
                                           for (auto& x : aRows.rows)
                                               aHandler(MySQL::Row((MYSQL_ROW)x.data(), x.size(), aRows.meta));
                                       });
                                       m_VerifyUse = true;
                                   } else if (m_SQL[m_Serial].query.compare(0, 6, "SELECT") == 0) {
                                       When(Method(m_Mock, Use)).Do([this](auto&& aHandler) {
                                           BOOST_TEST_MESSAGE("MOCK: no results");
                                       });
                                       m_VerifyUse = true;
                                   }
                               },
                               [this](const Error& aError) {
                                   throw aError;
                               }},
                           m_SQL[m_Serial].result);
            });

            When(Method(m_Mock, ensure)).AlwaysDo([]() { BOOST_TEST_MESSAGE("MOCK: ensure connection alive"); });
            When(Method(m_Mock, close)).AlwaysDo([]() { BOOST_TEST_MESSAGE("MOCK: connection closed"); });
        }

        ~Mock()
        {
            verifyUse();
            BOOST_CHECK_MESSAGE(m_SQL.size() == m_Serial, "MOCK: all SQL checks are done");
        }

        operator ConnectionFace*()
        {
            return &m_Mock.get();
        }
    };
} // namespace MySQL
