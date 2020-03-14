#pragma once

#include <boost/process.hpp>

namespace Util
{
    template<class... T>
    boost::process::child Spawn(T&&... t)
    {
        namespace bp = boost::process;
        bp::child sChild(t...);
        if (!sChild.running())
            throw std::runtime_error("fail to spawn, exit code: " + std::to_string(sChild.exit_code()));
        return sChild;
    }
}