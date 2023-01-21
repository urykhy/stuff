#define ASIO_HTTP_LIBRARY_HEADER

#include "MetricsDiscovery.hpp"
#include <asio_http/Client.hpp>
#include <asio_http/Server.hpp>
#include <asio_http/v2/Client.hpp>
#include <asio_http/v2/Server.hpp>
#include <format/Hex.hpp>
#include <jwt/JWT.hpp>
#include <prometheus/API.hpp>
#include <resource/Get.hpp>
#include <resource/Server.hpp>
#include <sd/Balancer.hpp>
#include <sd/Breaker.hpp>
#include <sd/NotifyWeight.hpp>
#include <sentry/Client.hpp>
