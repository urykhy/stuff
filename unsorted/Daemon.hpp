#pragma once

#include <grp.h>
#include <linux/limits.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace Util {
    void switchUser(const std::string& aUser)
    {
        struct passwd* sInfo = getpwnam(aUser.c_str());
        if (sInfo == nullptr) {
            throw std::runtime_error("getpwnam");
        }
        std::vector<gid_t> sGids;
        sGids.resize(NGROUPS_MAX);
        int sGroups = NGROUPS_MAX;
        if (getgrouplist(aUser.c_str(), sInfo->pw_gid, sGids.data(), &sGroups) == -1) {
            throw std::runtime_error("getgrouplist");
        }
        sGids.resize(sGroups);

        if (setgroups(sGids.size(), sGids.data())) {
            throw std::runtime_error("setgroups");
        }
        if (setgid(sInfo->pw_gid)) {
            std::runtime_error("getgid");
        }
        if (setuid(sInfo->pw_uid)) {
            std::runtime_error("getuid");
        }

        prctl(PR_SET_DUMPABLE, 1);
    }

    void setSignal(int aSignal, void (*aHandler)(int), int aFlags = SA_RESTART)
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = aHandler;
        sa.sa_flags   = aFlags;
        sigemptyset(&sa.sa_mask);
        sigaction(aSignal, &sa, NULL);
    }
} // namespace Util