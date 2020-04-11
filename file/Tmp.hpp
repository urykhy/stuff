#pragma once

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <filesystem>
#include <exception/Error.hpp>

namespace File
{
    class Tmp
    {
        int m_FD = -1;

    public:
        using Error = Exception::Error<Tmp>;

        Tmp(const Tmp&) = delete;
        Tmp(Tmp&& aOther)
        : m_FD(aOther.m_FD)
        {
            aOther.m_FD = -1;
        }

        Tmp& operator=(const Tmp&) = delete;
        Tmp& operator=(Tmp&& aOther)
        {
            close();
            m_FD = aOther.m_FD;
            aOther.m_FD = -1;
            return *this;
        }

        Tmp(const std::string& aName)
        {
            auto sPath = std::filesystem::temp_directory_path();
            sPath /= aName + ".XXXXXX";
            std::string sTmp = sPath.native();

            m_FD = mkstemp(sTmp.data());
            if (m_FD == -1)
                throw Error("File::Tmp: fail to create tmp file");
        }
        ~Tmp() throw()
        {
            close();
        }

        void close()
        {
            if (m_FD != -1)
            {
                std::error_code ec; // avoid throw
                std::filesystem::remove(filename(), ec);
                ::close(m_FD);
                m_FD = -1;
            }
        }

        void write(const void* aPtr, ssize_t aSize)
        {
            ssize_t sLen = ::write(m_FD, aPtr, aSize);
            if (sLen != aSize)
                throw Error(Exception::with_errno("File:Tmp: fail to write into tmp file", errno));
        }

        std::string filename() const
        {
            std::string sTmp(PATH_MAX, '\0');
            std::string sLink = "/proc/self/fd/" + std::to_string(m_FD);
            int sSize = readlink(sLink.c_str(), sTmp.data(), sTmp.size());
            if (sSize == -1)
                throw Error(Exception::with_errno("File:Tmp: fail to get tmp file name", errno));
            sTmp.resize(sSize);
            return sTmp;
        }
    };
} // namespace File