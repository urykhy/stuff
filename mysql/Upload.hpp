#pragma once

#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>

#define FILE_NO_ARCHIVE
#include <file/Dir.hpp>
#include <file/File.hpp>
#include <threads/Group.hpp>
#include <time/Time.hpp>
#include <unsorted/Log4cxx.hpp>
#include <unsorted/Raii.hpp>

#include "Client.hpp"

namespace MySQL::Upload {
    class Worker
    {
        const std::string     m_BaseFolder;
        const MySQL::Config   m_Config;
        volatile bool         m_Stop = false;
        std::atomic<uint32_t> m_Size{0};
        enum
        {
            DELAY_ON_ERROR = 10
        };

        using FileList = std::vector<std::string>;

        FileList update()
        {
            FileList sResult = File::listFiles(m_BaseFolder, ".sql");
            std::sort(sResult.begin(), sResult.end());
            return sResult;
        }

        void upload(const std::string& sFilename)
        {
            INFO("start uploading " << sFilename);
            MySQL::Connection sClient(m_Config);
            sClient.Query("BEGIN");
            File::by_string(sFilename, [&sClient](auto sView) {
                sClient.Query(std::string(sView));
            });
            std::filesystem::remove(sFilename);
            sClient.Query("COMMIT");
            INFO("success");
        }

        void worker()
        {
            time_t sLastError = 0;
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
                for (const auto& x : sFiles) {
                    try {
                        upload(x);
                        m_Size--;
                    } catch (const std::exception& e) {
                        WARN("failed: " << e.what());
                        sLastError = time(NULL);
                    }
                }
            }
        }

    public:
        Worker(const std::string aBaseFolder, const Config& aConfig)
        : m_BaseFolder(aBaseFolder)
        , m_Config(aConfig)
        {}

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this]() { worker(); });
            aGroup.at_stop([this]() { m_Stop = true; });
        }

        uint32_t size() const { return m_Size; }
    };

    class Queue
    {
        const std::string m_BaseFolder;

    public:
        Queue(const std::string aBaseFolder)
        : m_BaseFolder(aBaseFolder)
        {}

        using List = std::list<std::string>;
        void push(const List& aData)
        {
            Time::Zone        t(cctz::utc_time_zone());
            const std::string sFileName = t.format(::time(NULL), Time::ISO);
            const std::string sTmpName  = m_BaseFolder + "/" + sFileName + ".tmp";
            const std::string sName     = m_BaseFolder + "/" + sFileName + ".sql";

            Util::Raii sGuard([&sTmpName]() {
                std::error_code ec; // avoid throw
                std::filesystem::remove(sTmpName, ec);
            });
            File::write(sTmpName, [&aData](File::IWriter* aWriter) {
                for (auto& x : aData) {
                    aWriter->write(x.c_str(), x.size());
                    aWriter->write("\n", 1);
                }
            });
            std::filesystem::rename(sTmpName, sName);
            sGuard.dismiss();
            INFO("queued file " << sName);
        }
    };
} // namespace MySQL::Upload
