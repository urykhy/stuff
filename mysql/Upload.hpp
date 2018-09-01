#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <iostream> // for std::cerr

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include <Client.hpp>
#include <../time/Time.hpp>
#include <Periodic.hpp> // for sleep
#include <Group.hpp>

namespace MySQL::Upload
{
    class Worker
    {
        const std::string m_BaseFolder;
        const MySQL::Config m_Config;
        volatile bool m_Stop = false;
        volatile uint32_t m_Size = 0;
        enum { DELAY_ON_ERROR = 10 };

        using FileList = std::vector<std::string>;

        FileList update()
        {
            namespace fs = boost::filesystem;
            FileList sResult;

            if (!fs::exists(m_BaseFolder) || !fs::is_directory(m_BaseFolder)) {
                return sResult;
            }

            const fs::directory_iterator sEnd;
            for (fs::directory_iterator sIter(m_BaseFolder) ; sIter != sEnd ; ++sIter)
            {
                if (!fs::is_regular_file(sIter->status()))
                    continue;
                const std::string sName = sIter->path().native();
                if (boost::algorithm::ends_with(sName, ".sql"))
                    sResult.push_back(sName);
            }

            std::sort(sResult.begin(), sResult.end());
            return sResult;
        }

        void upload(const std::string& sFileName)
        {
            MySQL::Connection sClient(m_Config);
            sClient.Query("BEGIN");

            std::string sBuffer;
            std::ifstream sFile(sFileName);
            while (std::getline(sFile, sBuffer)) {
                sClient.Query(sBuffer);
            }
            boost::filesystem::remove(sFileName);
            sClient.Query("COMMIT");
        }

        void worker()
        {
            time_t sLastError = 0;
            while (!m_Stop)
            {
                if (sLastError + DELAY_ON_ERROR > time(NULL))
                {
                    Threads::sleep(0.5);
                    continue;
                }
                const auto sFiles = update();
                if (sFiles.empty())
                {
                    Threads::sleep(0.5);
                    continue;
                }
                m_Size = sFiles.size();
                for (const auto& x : sFiles)
                    try {
                        std::cerr << "Worker: starting " << x << std::endl;
                        upload(x);
                        std::cerr << "Worker: uploaded " << std::endl;
                        m_Size--;
                    } catch (const std::exception& e) {
                        std::cerr << "Upload error: " << e.what() << std::endl;
                        sLastError = time(NULL);
                    }
            }
        }

    public:
        Worker(const std::string aBaseFolder, const Config& aConfig) : m_BaseFolder(aBaseFolder), m_Config(aConfig) { }

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this](){ worker(); });
            aGroup.at_stop([this](){ m_Stop = true; });
        }

        uint32_t size() const { return m_Size; }
    };

    class Queue
    {
        const std::string m_BaseFolder;
    public:
        Queue(const std::string aBaseFolder) : m_BaseFolder(aBaseFolder) {}

        using List = std::list<std::string>;
        void push(const List& aData)
        {
            Time::Zone t(cctz::utc_time_zone());
            const std::string sFileName = t.format(::time(NULL), Time::ISO);
            std::ofstream sFile(m_BaseFolder + "/" + sFileName + ".sql");
            sFile.exceptions(std::ifstream::failbit);

            std::copy(aData.begin(), aData.end(), std::ostream_iterator<std::string>(sFile, "\n"));
            std::cerr << "Queue: written " << sFileName << std::endl;
        }
    };
}
