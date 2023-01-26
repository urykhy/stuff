#pragma once

#include <map>

#include <boost/any.hpp>
#include <boost/core/noncopyable.hpp>

#include "Metrics.hpp"

namespace Prometheus {
    class GetOrCreate : public boost::noncopyable
    {
        using Lock = std::unique_lock<std::mutex>;
        std::mutex m_Mutex;

        std::map<std::string, boost::any> m_Map;

    public:
        template <class T>
        T* get(const std::string& aName)
        {
            Lock sLock(m_Mutex);
            using Ptr  = std::shared_ptr<T>;
            auto& sVal = m_Map[aName];
            if (sVal.empty())
                sVal = std::make_shared<T>(aName);
            return boost::any_cast<Ptr>(sVal).get();
        }

        void erase(const std::string& aName)
        {
            Lock sLock(m_Mutex);
            m_Map.erase(aName);
        }
    };
} // namespace Prometheus