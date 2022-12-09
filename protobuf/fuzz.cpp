#include "Protobuf.hpp"
#include "tutorial.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    thread_local std::pmr::monotonic_buffer_resource sPool(1024 * 1024);

    try {
        std::string_view    sInput(reinterpret_cast<const char*>(data), size);
        pmr_tutorial::xtest sTmp(&sPool);
        sTmp.ParseFromString(sInput);
    } catch (...) {
    }

    sPool.release();
    return 0;
}
