#pragma once

#include "Group.hpp"

namespace Threads
{
    // apply Handler to every data element
    // every thread process own `partition`
    //
    // P must return partition id

    template<class D, class P, class H>
    void ForEach(const D& aData, P&& aPartition, H&& aHandler, unsigned aCount = 4)
    {
        Group sGroup;
        for (unsigned sPart = 0; sPart < aCount; sPart++)
            sGroup.start([&aData, &aPartition, &aHandler, sPart, aCount]()
            {
                for (auto& x : aData)
                    if (aPartition(x) % aCount == sPart)
                        aHandler(x);
            });
    }

} // namespace Threads
