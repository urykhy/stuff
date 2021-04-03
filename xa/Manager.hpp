#pragma once

#include "Store.hpp"

namespace XA {

    class Manager
    {
        std::vector<Store*> m_Alive;
        std::vector<Store*> m_Delay;
        std::vector<Store*> m_Repair;

        Store m_Store;
    public:



    };

} // namespace XA