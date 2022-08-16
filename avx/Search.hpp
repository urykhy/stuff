#pragma once

#include <stdint.h>
#include <sys/mman.h>

#include <cmath>
#include <iostream>

#include "Wide.hpp"

namespace Util {
    inline int avxLowerBound1(const WideInt32& aInput, uint32_t aVal)
    {
        WideInt32 sTmp(aVal);
        sTmp >= aInput;
        int sPos = __builtin_ffs(~_mm256_movemask_ps((__m256)sTmp.data)) - 1;
        return sPos;
    }

    inline int avxLowerBound4(const WideInt32& aInput, uint32_t aVal)
    {
        auto*    sStart = reinterpret_cast<const uint32_t*>(&aInput);
        uint64_t sMask  = 0;
        for (int i = 0; i < 4; i++) {
            WideInt32 sTmp(aVal);
            // 32 = sizeof(uint32_t) in bits
            sTmp >= *reinterpret_cast<const WideInt32*>(sStart + i * (256 / 32));
            uint64_t sStepMask = _mm256_movemask_ps((__m256)sTmp.data);
            sMask |= (sStepMask << (i * 8));
        }
        int sPos = __builtin_ffs(~sMask) - 1;
        return sPos > -1 ? sPos : 32;
    }

    // https://stackoverflow.com/questions/60169819/modern-approach-to-making-stdvector-allocate-aligned-memory
    template <typename T>
    struct AlignedAllocator
    {
        using value_type = T;
        static std::align_val_t constexpr ALIGNMENT{64};

        template <class U>
        struct rebind
        {
            using other = AlignedAllocator<U>;
        };

        [[nodiscard]] value_type* allocate(std::size_t aCount)
        {
            auto const sSize = aCount * sizeof(value_type);
            auto*      sPtr  = reinterpret_cast<value_type*>(::operator new[](sSize, ALIGNMENT));
            return sPtr;
        }
        void deallocate(value_type* aPtr, [[maybe_unused]] std::size_t aSize)
        {
            ::operator delete[](aPtr, ALIGNMENT);
        }
    };

    template <typename T>
    using alignedVector = std::vector<T, AlignedAllocator<T>>;

    // recursive index
    class avxIndex
    {
        const alignedVector<uint32_t>& m_Input;
        static constexpr unsigned      INDEX_SIZE = 8; // 256 bit / 32 (sizeof uint32)

        std::vector<alignedVector<uint32_t>> m_Index;
        int                                  m_TopLevel = 0;

    public:
        avxIndex(const alignedVector<uint32_t>& aInput)
        : m_Input(aInput)
        {
            assert(aInput.size() % INDEX_SIZE == 0); // FIXME
            m_Index.reserve(32);                     // FIXME

            const alignedVector<uint32_t>* sInput = &aInput;

            while (true) {
                size_t                  sInputSize = sInput->size() / INDEX_SIZE;
                alignedVector<uint32_t> sIndex;
                sIndex.reserve(sInputSize);
                for (unsigned i = 0; i < sInputSize; i++)
                    sIndex.push_back(sInput->operator[]((i + 1) * INDEX_SIZE - 1));
                m_Index.push_back(std::move(sIndex));
                sInput = &m_Index.back();
                if (sInput->size() == INDEX_SIZE) // top level index built
                {
                    m_TopLevel = m_Index.size() - 1;
                    break;
                }
                assert(m_Index.size() <= 32);
            }
        }

        int lower_bound(uint32_t aValue)
        {
            int sOffset = 0;
            int sPos    = 0;

            for (int i = m_TopLevel; i >= 0; i--) {
                const auto& sIndex = m_Index[i];
                sPos = avxLowerBound1(*reinterpret_cast<const WideInt32*>(&sIndex[sOffset]), aValue);
                // FIXME: handle nx
                sOffset *= INDEX_SIZE;
                sOffset += sPos * INDEX_SIZE;
            }

            // search over original data
            sPos = avxLowerBound1(*reinterpret_cast<const WideInt32*>(&m_Input[sOffset]), aValue);
            // FIXME: handle nx
            return sPos + sOffset;
        }

        size_t memory() const
        {
            size_t sUsage = 0;
            for (auto& x : m_Index) {
                sUsage += x.size() * sizeof(uint32_t);
            }
            return sUsage;
        }

        int height() const
        {
            return m_TopLevel;
        }
    };

} // namespace Util