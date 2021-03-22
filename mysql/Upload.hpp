#pragma once

#include <string>
#include <vector>

#include <boost/algorithm/string/predicate.hpp>

#define FILE_NO_ARCHIVE
#include "Client.hpp"

#include <file/Dir.hpp>
#include <file/File.hpp>
#include <threads/DiskQueue.hpp>
#include <unsorted/Log4cxx.hpp>
#include <unsorted/Raii.hpp>

namespace MySQL::Upload {

    using List = std::list<std::string>;

    class Consumer
    {
        Threads::DiskQueue::Consumer m_Consumer;
        const std::string            m_LogTable;
        const MySQL::Config          m_Config;

        void upload(const std::string& sName)
        {
            INFO("start uploading " << sName);
            bool              sAlready  = false;
            const std::string sFileName = File::getFilename(sName);
            MySQL::Connection sClient(m_Config);

            sClient.Query("BEGIN");
            sClient.Query("SELECT 1 FROM " + m_LogTable + " WHERE name = '" + sFileName + "'");
            sClient.Use([&sAlready](MySQL::Row&&) { sAlready = true; });
            if (sAlready) {
                INFO("already uploaded");
                return;
            }
            sClient.Query("INSERT INTO " + m_LogTable + "(name) VALUES('" + sFileName + "')");
            File::by_string(sName, [&sClient](auto sView) {
                sClient.Query(std::string(sView));
            });
            sClient.Query("COMMIT");
            INFO("success");
        }

    public:
        Consumer(const std::string& aBase, const std::string& aLogTable, const Config& aConfig)
        : m_Consumer({.base = aBase, .ext = ".upload.sql"}, [this](const std::string& aName) { upload(aName); })
        , m_LogTable(aLogTable)
        , m_Config(aConfig)
        {}

        void     start(Threads::Group& aGroup) { m_Consumer.start(aGroup); }
        uint32_t size() const { return m_Consumer.size(); }
    };

    class Producer
    {
        Threads::DiskQueue::Producer m_Producer;

    public:
        Producer(const std::string aBase)
        : m_Producer({.base = aBase, .ext = "upload.sql"})
        {}

        void push(const std::string& aName, const List& aData)
        {
            m_Producer.push(aName, [aData](auto* aFile) {
                for (auto& x : aData) {
                    aFile->write(x.c_str(), x.size());
                    aFile->write("\n", 1);
                }
            });
        }
    };
} // namespace MySQL::Upload
