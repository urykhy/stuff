#pragma once
#include <map>
#include <string>

namespace Cache {

    // http://www.codinginlondon.com/2009/05/cheap-ioc-in-native-c.html

    class IoC
    {
        std::map<std::string, void*> typeInstanceMap;
    public:
        template<class T> void Put(const T& object)
        {
            typeInstanceMap[typeid(T).name()] = (void*)&object;
        }
        template<class T> T& Get()
        {
            return *((T*)typeInstanceMap[typeid(T).name()]);
        }
    };
}
