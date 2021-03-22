#pragma once

#define FILE_NO_ARCHIVE
#include "Group.hpp"

#include <file/Dir.hpp>
#include <file/File.hpp>
#include <threads/Group.hpp>
#include <unsorted/Log4cxx.hpp>
#include <unsorted/Raii.hpp>
#include <unsorted/Random.hpp>

namespace Threads::DiskQueue {

    struct Params
    {
        std::string base = "/tmp";
        std::string ext  = ".bin";
    };

    struct Consumer
    {
        using Handler = std::function<void(const std::string&)>;

    private:
        const Params m_Params;
        Handler      m_Handler;

        std::atomic<bool>     m_Stop{false};
        std::atomic<uint32_t> m_Size{0};
        enum
        {
            DELAY_ON_ERROR = 10
        };

        using FileList = std::vector<std::string>;

        FileList update()
        {
            FileList sResult = File::listFiles(m_Params.base, m_Params.ext);
            std::sort(sResult.begin(), sResult.end());
            return sResult;
        }

        void worker()
        {
            time_t sLastError = 0;
            log4cxx::NDC ndc("disk queue");

            while (!m_Stop) {
                if (sLastError + DELAY_ON_ERROR > time(NULL)) {
                    Threads::sleep(0.5);
                    continue;
                }
                const auto sFiles = update();
                if (sFiles.empty()) {
                    Threads::sleep(0.5);
                    continue;
                }
                m_Size = sFiles.size();
                try {
                    for (const auto& x : sFiles) {
                        m_Handler(x);
                        m_Size--;
                        std::filesystem::remove(x);
                    }
                } catch (const std::exception& e) {
                    WARN("failed: " << e.what());
                    sLastError = time(NULL);
                }
            }
        }

    public:
        Consumer(const Params& aParams, Handler aHandler)
        : m_Params(aParams)
        , m_Handler(aHandler)
        {}

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this]() { worker(); });
            aGroup.at_stop([this]() { m_Stop = true; });
        }

        uint32_t size() const { return m_Size; }
    };

    class Producer
    {
        const Params  m_Params;
        File::DirSync m_DirSync;

    public:
        Producer(const Params& aParams)
        : m_Params(aParams)
        , m_DirSync(m_Params.base)
        {
        }

        template <class T>
        void push(const std::string& aName, const T& aWriter)
        {
            log4cxx::NDC ndc("disk queue");

            const std::string sName = m_Params.base + "/" + aName + '.' + m_Params.ext;
            if (std::filesystem::exists(sName)) {
                INFO("already exists " << sName);
                return;
            }

            const std::string sTmpName = m_Params.base + "/." + aName + '#' + Util::randomStr() + ".tmp";

            Util::Raii sGuard([&sTmpName]() {
                std::error_code ec; // avoid throw
                std::filesystem::remove(sTmpName, ec);
            });

            INFO("writing to tmp file " << sTmpName);
            File::write(
                sTmpName, [&aWriter](File::IWriter* aFile) {
                    aWriter(aFile);
                    aFile->sync();
                    aFile->close();
                },
                O_EXCL);

            std::filesystem::rename(sTmpName, sName);
            sGuard.dismiss();

            try {
                m_DirSync();
            } catch (...) {
                ;
            }
            INFO("queued file " << sName);
        }

        void cleanup()
        {
            for (auto& x : File::listFiles(m_Params.base, ".tmp"))
                std::filesystem::remove(x);
        }
    };

} // namespace Threads::DiskQueue