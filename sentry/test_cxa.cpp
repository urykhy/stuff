#include "Cxa.hpp"

int main(void)
{
    Sentry::InitCXA([](Sentry::Message& aMsg){
        aMsg.set_version("test.cpp","0.1");
        aMsg.set_environment("test");
        aMsg.set_level("info");
    });

    try {
        [](){ throw std::runtime_error("test exception"); }();
    } catch(const std::exception& e) { ;;  }

    return 0;
}