#pragma once

#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/ndc.h>

// create:    log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("service"));
// configure: log4cxx::PropertyConfigurator::configure("logger.conf");
// raii ndc:  log4cxx::NDC ndc("nested debug context");

#if 0
extern log4cxx::LoggerPtr sLogger;
#define TRACE(x) LOG4CXX_TRACE(sLogger, x)
#define DEBUG(x) LOG4CXX_DEBUG(sLogger, x)
#define INFO(x)  LOG4CXX_INFO(sLogger, x)
#define WARN(x)  LOG4CXX_WARN(sLogger, x)
#define ERROR(x) LOG4CXX_ERROR(sLogger, x)
#define FATAL(x) LOG4CXX_FATAL(sLogger, x)
#endif

namespace Logger
{
    void Configure(const std::string& aPath = "logger.conf")
    {
        log4cxx::PropertyConfigurator::configure(aPath);
    }

    log4cxx::LoggerPtr Get(const std::string& aName = "service")
    {
        return log4cxx::LoggerPtr(log4cxx::Logger::getLogger(aName));
    }
} // namespace Util