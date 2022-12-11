#pragma once
#include <memory>

#include "cbor-tuple.hpp"

#include <mpl/Mpl.hpp>

namespace cbor {
    namespace aux {
        struct proxy_i
        {
            virtual void run(cbor::istream& in, cbor::ostream& out) = 0;
            virtual ~proxy_i(){};
        };

        template <class F>
        class proxy : public proxy_i
        {
            F func;

        public:
            proxy(F f)
            : func(f)
            {
            }

            void run(cbor::istream& in, cbor::ostream& out) override
            {
                typename Mpl::signature<decltype(func)>::argument_type arg;
                cbor::read(in, arg);
                auto x = std::apply(func, arg);
                cbor::write(out, x);
            }
        };
    } // namespace aux
    using proxy_ptr = std::shared_ptr<aux::proxy_i>;

    template <class F>
    proxy_ptr make_proxy(F f)
    {
        return std::make_shared<aux::proxy<F>>(f);
    }
} // namespace cbor