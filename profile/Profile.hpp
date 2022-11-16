#pragma once

#include <filesystem>

#include <unsorted/Process.hpp>

namespace Profile {

    // perf fail to notice new threads if run via -p <pid>
    // so we capture all, and filter later
    void CPU(const std::string& aFilename, time_t aTime, unsigned aFreq = 100)
    {
        namespace bp = boost::process;

        auto sResult = Util::Spawn("/usr/bin/perf",
                                   "record", "-F", std::to_string(aFreq), "--call-graph", "lbr", "-e", "cpu-cycles",
                                   "-a",
                                   //"-p", std::to_string(getpid()),
                                   bp::std_in.close(),
                                   bp::std_out.close(),
                                   bp::std_err.close());
        sleep(aTime);
        kill(sResult.native_handle(), SIGINT);
        sResult.wait();

        std::error_code ec; // avoid throw
        std::filesystem::remove(aFilename, ec);
        boost::asio::io_service sAsio;
        boost::filesystem::path sOut(aFilename);
        std::string             sCmd   = "perf script --pid=" + std::to_string(getpid()) + " | stackcollapse-perf.pl | flamegraph.pl --title=\"CPU Cycles\"";
        auto                    sChild = Util::Spawn("/bin/bash", "-c", sCmd,
                                                     bp::std_in.close(),
                                                     bp::std_out > sOut,
                                                     bp::std_err.close(),
                                                     sAsio);
        sAsio.run();
        sChild.wait();
    }

} // namespace Profile