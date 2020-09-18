#pragma once

#include <stdlib.h>

#include <filesystem>

#include <log4cxx/basicconfigurator.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/logger.h>
#include <log4cxx/ndc.h>
#include <log4cxx/patternlayout.h>
#include <log4cxx/propertyconfigurator.h>

// raii ndc:  log4cxx::NDC ndc("nested debug context");

namespace Logger {
    inline log4cxx::LoggerPtr Get(const std::string& aName = "main")
    {
        return log4cxx::LoggerPtr(log4cxx::Logger::getLogger(aName));
    }

    inline log4cxx::LoggerPtr Prepare()
    {
        const char* sEnv = getenv("LOG4CXX");
        if (sEnv) {
            log4cxx::PropertyConfigurator::configure(sEnv);
        } else {
            if (std::filesystem::exists("logger.conf")) {
                log4cxx::PropertyConfigurator::configure("logger.conf");
            } else {
                auto consoleAppender = new log4cxx::ConsoleAppender(log4cxx::LayoutPtr(new log4cxx::PatternLayout("%d [%t] [%-5p] %m%n")));
                log4cxx::BasicConfigurator::configure(log4cxx::AppenderPtr(consoleAppender));
                log4cxx::Logger::getRootLogger()->setLevel(log4cxx::Level::getWarn());
            }
        }

        return Get();
    }
} // namespace Logger

inline log4cxx::LoggerPtr sLogger = Logger::Prepare();

#define TRACE(x) LOG4CXX_TRACE(sLogger, x)
#define DEBUG(x) LOG4CXX_DEBUG(sLogger, x)
#define INFO(x)  LOG4CXX_INFO(sLogger, x)
#define WARN(x)  LOG4CXX_WARN(sLogger, x)
#define ERROR(x) LOG4CXX_ERROR(sLogger, x)
#define FATAL(x) LOG4CXX_FATAL(sLogger, x)
