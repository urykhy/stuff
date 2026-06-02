#pragma once

#include <optional>
#include <string>
#include <vector>

#include <exprtk.hpp>

extern template class exprtk::symbol_table<double>;
extern template class exprtk::expression<double>;
extern template class exprtk::parser<double>;

namespace Util {
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
    };
} // namespace Util
