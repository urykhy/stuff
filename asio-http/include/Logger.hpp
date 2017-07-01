#pragma once
#include <log4cxx/logger.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/ndc.h>

extern log4cxx::LoggerPtr logger;
// create: log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("service"));
// configure: log4cxx::PropertyConfigurator::configure("logger.conf");
// raii ndc: log4cxx::NDC ndc("nested debug context");

#define TRACE(x) LOG4CXX_TRACE(logger, x)
#define DEBUG(x) LOG4CXX_DEBUG(logger, x)
#define INFO(x)  LOG4CXX_INFO(logger, x)
#define WARN(x)  LOG4CXX_WARN(logger, x)
#define ERROR(x) LOG4CXX_ERROR(logger, x)
