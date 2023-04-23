#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Reflection.hpp"

#include <exprtk.hpp>

namespace Protobuf {

    struct ExprTK
    {
        using symbol_table_t = exprtk::symbol_table<double>;
        using expression_t   = exprtk::expression<double>;
        using parser_t       = exprtk::parser<double>;
        using settings_t     = parser_t::settings_t;

        settings_t     m_Settings;
        symbol_table_t m_Table;
        expression_t   m_Expression;
        parser_t       m_Parser;

        struct Info
        {
            std::string           name;
            std::vector<uint32_t> path;
            double*               ptr = nullptr;
        };
        std::vector<Info> m_VarList;

        ExprTK()
        : m_Settings(settings_t::compile_all_opts)
        , m_Parser(m_Settings)
        {
            m_Expression.register_symbol_table(m_Table);
        }

        void compile(const std::string& aStr)
        {
            if (!m_Parser.compile(aStr, m_Expression))
                throw std::invalid_argument("Exprtk: fail to compile: " + m_Parser.error());
        }
        double eval()
        {
            return m_Expression.value();
        }

        template <class T>
        void resolveFrom(T& aVal)
        {
            std::vector<std::string> sVarList;
            m_Table.get_variable_list(sVarList);
            m_VarList.reserve(sVarList.size());

            using R = Reflection<T>;
            for (auto& x : sVarList) {
                typename R::Path sPath;
                R::walk(aVal, x, sPath);
                double* sPtr = &m_Table.get_variable(x)->ref();
                m_VarList.push_back({std::move(x), std::move(sPath), sPtr});
            }
        }

        template <class T>
        void assignFrom(T& aVal)
        {
            using R = Reflection<T>;
            for (auto& sInfo : m_VarList) {
                R::use(aVal, sInfo.path, [&sInfo](auto x) mutable {
                    if constexpr (
                        std::is_same_v<decltype(x), std::optional<int32_t>> or
                        std::is_same_v<decltype(x), std::optional<uint32_t>> or
                        std::is_same_v<decltype(x), std::optional<int64_t>> or
                        std::is_same_v<decltype(x), std::optional<uint64_t>> or
                        std::is_same_v<decltype(x), std::optional<float>> or
                        std::is_same_v<decltype(x), std::optional<double>>) {
                        *sInfo.ptr = (double)*x;
                    } else {
                        throw std::invalid_argument("Protobuf::ExprTK: not supported type for variable " + sInfo.name);
                    }
                });
            }
        }
    };
} // namespace Protobuf