#pragma once

#include <filesystem>

#include <unsorted/Process.hpp>

namespace Profile {

    // http://www.brendangregg.com/offcpuanalysis.html
    // use sudo
    void Off(const std::string& aFilename, time_t aTime)
    {
        namespace bp = boost::process;

        auto sResult = Util::Perform("sudo", "offwaketime-bpfcc", "-f", "-U", "-p", std::to_string(getpid()), std::to_string(aTime));
        if (sResult.code == 0) {
            std::error_code ec; // avoid throw
            std::filesystem::remove(aFilename, ec);
            boost::asio::io_service sAsio;
            bp::async_pipe          sIn(sAsio);
            boost::filesystem::path sOut(aFilename);
            auto                    sChild = Util::Spawn(bp::search_path("flamegraph.pl"),
                                      "--title=\"Off-CPU Time\"", "--countname=us", "--color=chain",
                                      bp::std_in<sIn, bp::std_out> sOut, sAsio);
            boost::asio::async_write(sIn, boost::asio::buffer(sResult.out), [&sIn](boost::system::error_code, size_t) { sIn.close(); });
            sAsio.run();
            sChild.wait();
        } else {
            throw std::runtime_error("fail to record profile: " + sResult.err);
        }
    }

    // perf fail to notice new threads if run via -p <pid>
    // so we capture all, and filter later
    void CPU(const std::string& aFilename, time_t aTime, unsigned aFreq = 100)
    {
        namespace bp = boost::process;

        auto sResult = Util::Spawn("/usr/bin/perf",
                                   "record", "-F", std::to_string(aFreq),
                                   "--call-graph", "fp",
                                   "-e", "cpu-cycles",
                                   "-a",
                                   //"-p", std::to_string(getpid()),
                                   bp::std_in.close(), bp::std_out.close(), bp::std_err.close());
        sleep(aTime);
        kill(sResult.native_handle(), SIGINT);
        sResult.wait();

        std::error_code ec; // avoid throw
        std::filesystem::remove(aFilename, ec);
        boost::asio::io_service sAsio;
        boost::filesystem::path sOut(aFilename);
        std::string             sCmd   = "perf script --pid=" + std::to_string(getpid()) + " | stackcollapse-perf.pl | flamegraph.pl --title=\"CPU Cycles\"";
        auto                    sChild = Util::Spawn("/bin/bash", "-c", sCmd, bp::std_in.close(), bp::std_out > sOut, bp::std_err.close(), sAsio);
        sAsio.run();
        sChild.wait();
    }

} // namespace Profile