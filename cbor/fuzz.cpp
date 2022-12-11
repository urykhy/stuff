#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#define FUZZING_BUILD_MODE
#include "cbor.hpp"

bool init()
{
    // make coverage happy
    cbor::omemstream out;
    cbor::write(out, (uint8_t)1, 5.0f, 5.0, true, false,
                std::string("string"), std::string_view("string"), "string",
                -1, __UINT8_MAX__, __UINT16_MAX__, __UINT32_MAX__, ((uint64_t)__UINT32_MAX__) * 2);
    cbor::write_tag(out, 1);
    return true;
}

template <class T>
void test(const cbor::binary& aInput)
{
    try {
        T sVal;
        cbor::from_string(aInput, sVal);
    } catch (...) {
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static bool  sInit = init();
    cbor::binary sInput(reinterpret_cast<const char*>(data), size);

    test<bool>(sInput);
    test<float>(sInput);
    test<double>(sInput);
    test<std::string_view>(sInput);
    test<std::optional<std::string>>(sInput);
    test<std::list<std::string>>(sInput);
    test<std::vector<double>>(sInput);
    test<std::vector<std::string>>(sInput);
    test<std::map<uint64_t, int64_t>>(sInput);
    test<std::unordered_map<float, double>>(sInput);

    try {
        uint64_t         sTag;
        bool             sVal;
        cbor::imemstream sStream(sInput);
        cbor::read_tag(sStream, sTag);
        cbor::read(sStream, sVal);
    } catch (...) {
    }

    return 0;
}
