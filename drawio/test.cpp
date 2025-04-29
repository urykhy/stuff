#define BOOST_TEST_MODULE Suites
#include "test.hpp"

#include <boost/test/unit_test.hpp>

class Worker : public drawio::UserInfoLoader::API
{
    // worker state
    std::string m_Name;
    // std::list<drawio::UserInfoLoader::UserInfo> m_Info;

public:
    void Download(const std::string& aName) override
    {
        BOOST_TEST_MESSAGE("download " << aName);
        m_Name = aName;
    }
    void Parse() override
    {
        BOOST_TEST_MESSAGE("parse " << m_Name);
    }
    void Group() override
    {
        BOOST_TEST_MESSAGE("group " << m_Name);
    }
    void Upload() override
    {
        BOOST_TEST_MESSAGE("upload to s3 " << m_Name);
    }
    void Statistics() override
    {
        BOOST_TEST_MESSAGE("update mysql stats " << m_Name);
    }
    void Finalize() override
    {
        BOOST_TEST_MESSAGE("finalize " << m_Name);
    }
    void Reset() override
    {
        BOOST_TEST_MESSAGE("reset after " << m_Name);
        m_Name.clear();
    }

    virtual ~Worker(){};
};

BOOST_AUTO_TEST_SUITE(DrawIO)
BOOST_AUTO_TEST_CASE(simple)
{
    Worker                      sImpl;
    drawio::UserInfoLoader::Worker sWorker(&sImpl);
    sWorker("test");

    Prometheus::Manager::instance().onTimer();
    for (auto x : Prometheus::Manager::instance().toPrometheus())
        BOOST_TEST_MESSAGE(x);
}
BOOST_AUTO_TEST_SUITE_END()