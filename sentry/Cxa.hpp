#pragma once

#include "Client.hpp"
#include "Message.hpp"

namespace Sentry
{
    using Prepare = std::function<void(Message&)>;

    // init catch exceptions magic
    void InitCXA(const Prepare& aPrepare);

} // namespace Sentry