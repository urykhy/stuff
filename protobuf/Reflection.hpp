#pragma once

#include <string>
#include <type_traits>
#include <vector>

namespace Protobuf {

    template <class T>
    struct Reflection
    {
        template <typename U, typename = void>
        struct can_walk : std::false_type
        {};
        template <typename U>
        struct can_walk<U, std::void_t<decltype(&U::GetReflectionKey)>> : std::true_type
        {};

        static void walk(T& aItem, std::string aName, std::vector<uint32_t>& aPath)
        {
            std::string sRest = aName;
            auto        sPos  = aName.find('.');
            if (sPos == std::string::npos) {
                sRest = {};
            } else {
                sRest.erase(0, sPos + 1);
                aName = aName.substr(0, sPos);
            }
            auto sKey = T::GetReflectionKey(aName);
            if (sKey == nullptr)
                throw std::invalid_argument("Protobuf::Reflection: field " + aName + " not found");
            aPath.push_back(sKey->id);
            aItem.GetByID(sKey->id, [&sRest, &aPath](auto x) mutable {
                using V = typename decltype(x)::value_type;
                if constexpr (can_walk<V>::value)
                    Reflection<V>::walk(*x, sRest, aPath);
            });
        }

        template <class H>
        static void use(T& aItem, const std::vector<uint32_t>& aPath, H&& aHandler, uint32_t aPos = 0)
        {
            aItem.GetByID(aPath[aPos], [&aPath, &aHandler, aPos](auto x) mutable {
                using V = typename decltype(x)::value_type;
                if constexpr (can_walk<V>::value)
                    Reflection<V>::use(*x, aPath, aHandler, aPos + 1);
                else
                    aHandler(x);
            });
        }
    };

} // namespace Protobuf