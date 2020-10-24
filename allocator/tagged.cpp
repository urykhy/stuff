#define BOOST_TEST_MODULE Suites

#include <boost/test/unit_test.hpp>

#include <dlfcn.h>
#include <malloc.h>

#include <mutex>
#include <string>

namespace Tagged {
    static inline thread_local unsigned gAllocatorTag = 0;

    struct Pair
    {
        std::atomic_size_t total = 0;
        std::atomic_size_t alive = 0;
    };
    static std::array<Pair, 256> gStats{256};

    void Alloc(unsigned aTag, size_t aSize)
    {
        auto& sPair = gStats[aTag];
        sPair.total += aSize;
        sPair.alive += aSize;
    }

    void Free(unsigned aTag, size_t aSize)
    {
        auto& sPair = gStats[aTag];
        sPair.alive -= aSize;
    }

    void Stats()
    {
        std::cout << "*** Tagged Memory Stats Begin" << std::endl;
        for (unsigned sTag = 0; sTag < 256; sTag++) {
            const auto& sPair = gStats[sTag];
            if (sPair.total > 0)
                std::cout << sTag << '\t' << sPair.alive << '\t' << sPair.total << std::endl;
        }
        std::cout << "*** Tagged Memory Stats End" << std::endl;
    }

    class Guard
    {
        uint8_t m_Old = gAllocatorTag;

    public:
        Guard(uint8_t aTag) { gAllocatorTag = aTag; }
        ~Guard() { gAllocatorTag = m_Old; }
    };
} // namespace Tagged

extern "C" void* malloc(size_t aSize)
{
    static auto libc_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");

    auto  sPtr  = (unsigned char*)libc_malloc(aSize + 1);
    auto  sSize = malloc_usable_size(sPtr);
    auto& sTag  = sPtr[sSize - 1];
    sTag        = Tagged::gAllocatorTag;
    Tagged::Alloc(sTag, sSize);
    return sPtr;
}

// realloc ?

extern "C" void free(void* aPtr)
{
    static auto libc_free = (void (*)(void*))dlsym(RTLD_NEXT, "free");

    auto sSize = malloc_usable_size(aPtr);
    if (sSize > 1) {
        auto  sPtr = (unsigned char*)aPtr;
        auto& sTag = sPtr[sSize - 1];
        Tagged::Free(sTag, sSize);
    }
    libc_free(aPtr);
}

BOOST_AUTO_TEST_CASE(tagged)
{
    BOOST_TEST_MESSAGE("start");
    {
        Tagged::Guard sGuard(1);
        std::string   sBuf(1024, ' ');
    }
    {
        Tagged::Guard sGuard(2);
        free(malloc(12345));
    }
    Tagged::Stats();
    BOOST_TEST_MESSAGE("finish");
}