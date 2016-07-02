
#include "logger.hpp"

#include <algorithm>
#include <iomanip>
#include <list>
#include <sys/time.h>
#include <link.h>
#include <dlfcn.h>

namespace Logger {

    __thread bool State::enabled;

    const char* levels[MAX_LEVEL] = {
        "CRIT ",
        "ERROR",
        "WARN ",
        "INFO ",
        "DEBUG",
        "TRACE",
    };

    typedef std::pair<struct log_state*, struct log_state*> ModuleInfo;
    static std::list<ModuleInfo>module_info;

    void add_module(struct log_state* a, struct log_state* b)
    {
        module_info.push_back(std::make_pair(a,b));
    }
    void delete_module(struct log_state* a, struct log_state* b)
    {
        module_info.erase(
                          std::remove(module_info.begin(), module_info.end(), std::make_pair(a,b)),
                          module_info.end());
    }

    static int cb(struct dl_phdr_info *info, size_t size, void *data)
    {
        const char * name = info->dlpi_name;
        void* obj = dlopen(name, RTLD_LAZY | RTLD_NOLOAD);
        if (obj)
        {
            void* sym1 = dlsym(obj, "__start___logger");
            void* sym2 = dlsym(obj, "__stop___logger");
            if (sym1 && sym2)
            {
                add_module((struct log_state*)sym1, (struct log_state*)sym2);
            }
            dlclose(obj);
        }
        return 0;
    }

    void init()
    {
        dl_iterate_phdr(cb, 0);
    }

    // control dynamic logger
    void add(const std::string& s, int level)
    {
        for (const auto& i : module_info) {
            for (log_state* x = i.first; x != i.second; x++)
            {
                if (s == x->func) {
                    x->level = level;
                }
            }
        }
    }
    void remove(const std::string& s)
    {
        for (const auto& i : module_info) {
            for (log_state* x = i.first; x != i.second; x++)
            {
                if (s == x->func) {
                    x->level = 0;
                }
            }
        }
    }
    void set_all(int level)
    {
        for (const auto& i : module_info) {
            for (log_state* x = i.first; x != i.second; x++)
            {
                x->level = level;
            }
        }
    }

    struct TimeFormatter
    {
        struct timeval tv;
        struct tm result;
        std::ostringstream m;

        const char* operator()()
        {
            m.str("");
            gettimeofday(&tv, NULL);
            localtime_r(&tv.tv_sec, &result);

            m << result.tm_year+1900 << '-'
            << std::setfill('0') << std::setw(2) << result.tm_mon + 1 << '-'
            << std::setw(2) << result.tm_mday << ' '
            << std::setw(2) << result.tm_hour << ':'
            << std::setw(2) << result.tm_min  << ':'
            << std::setw(2) << result.tm_sec  << '.'
            << std::setw(3) << int(tv.tv_usec/1000) << ' ';
            return m.str().data();
        }
    };

    const char* format_time()
    {
        static __thread TimeFormatter formatter;
        return formatter();
    }
}

