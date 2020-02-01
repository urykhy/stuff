#include "Cxa.hpp"

int main(void)
{
    Sentry::Client::Params sParams;
    sParams.url="web.sentry.docker:9000/api/2/store/";
    sParams.key="626d891753d6489ba426baa41d7c79fc";
    sParams.secret="350776c0cfba4013a93275e9de63ba5d";

    Sentry::InitCXA(sParams, [](Sentry::Message& aMsg){
        aMsg.set_version("test.cpp","0.1");
        aMsg.set_environment("test");
        aMsg.set_level("info");
    });

    try {
        [](){ throw std::runtime_error("test exception"); }();
    } catch(const std::exception& e) { ;;  }

    return 0;
}