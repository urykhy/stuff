#pragma once

#include <boost/asio.hpp>
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

    struct XResult
    {
        int code = 0;
        std::string out;
        std::string err;
    };

    template<class... T>
    XResult Perform(const std::string& aCmd, T&&... t)
    {
        namespace bp = boost::process;
        std::future<std::string> out;
        std::future<std::string> err;

        boost::asio::io_service sAsio;
        bp::child sChild = Spawn(bp::search_path(aCmd)
                               , bp::std_in.close()
                               , bp::std_out > out
                               , bp::std_err > err
                               , sAsio
                               , t...);
        sAsio.run();
        sChild.wait();

        return XResult{sChild.exit_code(), out.get(), err.get()};
    }
}