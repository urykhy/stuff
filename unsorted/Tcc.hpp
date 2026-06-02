#pragma once

#include <libtcc.h>

#include <stdexcept>
#include <string>

namespace Util {
    class TinyCompiler
    {
        TCCState* m_State = nullptr;

    public:
        TinyCompiler()
        : m_State(tcc_new())
        {
            if (!m_State) {
                throw std::runtime_error("fail to create tcc state");
            }
            tcc_set_output_type(m_State, TCC_OUTPUT_MEMORY);
        }

        ~TinyCompiler()
        {
            tcc_delete(m_State);
        }

        void Compile(const std::string& aCode)
        {
            if (tcc_compile_string(m_State, aCode.c_str()) == -1) {
                throw std::invalid_argument("fail to compile");
            }
            if (tcc_relocate(m_State, TCC_RELOCATE_AUTO) < 0) {
                throw std::invalid_argument("fail to relocate");
            }
        }

        void* GetSymbol(const std::string& aName)
        {
            return tcc_get_symbol(m_State, aName.c_str());
        }
    };
} // namespace Util
