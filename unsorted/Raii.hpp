#pragma once
#include <functional>

namespace Util
{
    struct Raii
    {
        typedef std::function<void ()> Handler;

        Raii(Handler h_) : h(h_), active(true)
        { }
        Raii(Raii&& rhs) : h(std::move(rhs.h)), active(rhs.active)
        { rhs.dismiss(); }

        ~Raii() throw() { if (active) h(); }
        void dismiss() throw () { active = false; }

    private:
        Handler h;
        bool active;
        Raii() = delete;
        Raii(const Raii&) = delete;
        Raii& operator=(const Raii&) = delete;
    };
}
