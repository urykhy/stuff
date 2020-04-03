#pragma once

#include <chrono>
#include "OrderedWorker.hpp"

using namespace std::chrono_literals;

namespace Threads
{
    // map/reduce over ListArray

    template<class Result, class D, class M, class R>
    Result MapReduce(const D& aData, M aMapper, R aReducer, unsigned aThreads = 4)
    {
        struct Task
        {
            const typename D::Array* input = nullptr;
            using MapResult = typename std::result_of<M(typename D::Array)>::type;
            MapResult result{};
        };
        std::list<Task> sTasks;
        aData.for_chunks([&sTasks](const auto& x){ sTasks.push_back({&x, {}}); });

        std::atomic<unsigned> sCount = 0;
        Result sResult{};
        OrderedWorker<Task*> sQueue(
            [aMapper](Task* t){ t->result = aMapper(*t->input); },
            [&sResult, &sCount, aReducer](Task* t){ aReducer(sResult, t->result); sCount++; }
        );
        Group sGroup;
        sQueue.start(sGroup, aThreads);

        for (auto& x: sTasks)
            sQueue.insert(&x);

        while (sCount < sTasks.size())
            std::this_thread::sleep_for(10ms);
        sGroup.wait();

        return sResult;
    }

} // namespace Threads