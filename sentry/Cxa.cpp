// hack to catch all exceptions
// based on https://github.com/rtbkit/rtbkit/blob/master/jml/arch/exception_hook.cc
#include "Cxa.hpp"

#include <dlfcn.h>
#include <exception>
#include <memory>
#include <unsorted/Demangle.hpp>

namespace Sentry
{
    typedef void (*cxa_throw_type) (void*, void*, void (*) (void *));
    static cxa_throw_type gRealHandler = 0;

    static bool gInitDone = false;
    void InitOverride()
    {
        void* sHandle = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);
        if (!sHandle) std::terminate();
        gRealHandler = (cxa_throw_type) dlvsym(sHandle, "__cxa_throw", "CXXABI_1.3");
        if (!gRealHandler) std::terminate();
        dlclose(sHandle);
        gInitDone = true;
    }

    static std::unique_ptr<Client> gClient;
    static Prepare gPrepare;
    void InitCXA(const Client::Params& aParams, const Prepare& aPrepare)
    {
        gClient = std::make_unique<Client>(aParams);
        gPrepare = aPrepare;
    }

    void hook(void* aObject, void* aInfo)
    {
        if (!gClient)
            return;

        std::string sWhat = "unknown message";
        const std::type_info* sInfo = static_cast<std::type_info*>(aInfo);
        if (typeid(std::exception).__do_catch(sInfo, &aObject, 0))
        {
            const std::exception* sStd  = static_cast<std::exception*>(aObject);
            sWhat = sStd->what();
        }

        Sentry::Message sMsg("__cxa_throw");
        sMsg.set_message("exception thrown");
        sMsg.set_exception(sWhat, Util::Demangle(sInfo->name()));
        sMsg.set_trace(GetStacktrace(), 3);
        gPrepare(sMsg);
        gClient->send(sMsg);
    }
}

extern "C" void __cxa_throw (void* aObject, void* aInfo, void (*aDtor) (void *))
{
    Sentry::hook(aObject, aInfo);
    if (!Sentry::gInitDone)
        Sentry::InitOverride();
    Sentry::gRealHandler(aObject, aInfo, aDtor);
    exit(0);
}