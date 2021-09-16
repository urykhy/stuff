#pragma once

#include <filesystem>
#include <memory>

#include "Writer.hpp"

#include <exception/Error.hpp>
#include <unsorted/Raii.hpp>

namespace File {
    class Tmp : public IWriter
    {
        std::string                 m_Name;
        std::unique_ptr<FileWriter> m_File;
        std::unique_ptr<BufWriter>  m_Writer;

    public:
        Tmp(const std::string& aName)
        {
            m_Name  = tmpName(std::filesystem::temp_directory_path() / aName, ".tmp-XXXXXX");
            int sFD = mkstemp(m_Name.data());
            if (sFD == -1)
                throw Exception::ErrnoError("File::Tmp: fail to create tmp file");

            Util::Raii sGuard([sFD]() { ::close(sFD); });
            m_File = std::make_unique<FileWriter>(sFD);
            sGuard.dismiss();
            m_Writer = std::make_unique<BufWriter>(m_File.get());
        }

        Tmp(Tmp&& aOld)
        : m_Name(std::move(aOld.m_Name))
        , m_File(std::move(aOld.m_File))
        , m_Writer(std::move(aOld.m_Writer))
        {}

        void write(const void* aPtr, size_t aSize) override
        {
            m_Writer->write(aPtr, aSize);
        }
        void flush() override
        {
            m_Writer->flush();
        }
        void sync() override
        {
            m_Writer->sync();
        }
        void close() override
        {
            m_Writer->close();
        }
        virtual ~Tmp() noexcept(false)
        {
            close();
            std::error_code ec; // avoid throw
            std::filesystem::remove(name(), ec);
        }

        const std::string& name() const { return m_Name; }
        size_t             size() { return std::filesystem::file_size(name()); }
    };
} // namespace File