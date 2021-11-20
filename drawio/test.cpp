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
    virtual void Parse()
    {
        BOOST_TEST_MESSAGE("parse " << m_Name);
    }
    virtual void Group()
    {
        BOOST_TEST_MESSAGE("group " << m_Name);
    }
    virtual void Upload()
    {
        BOOST_TEST_MESSAGE("upload to s3 " << m_Name);
    }
    virtual void Statistics()
    {
        BOOST_TEST_MESSAGE("update mysql stats " << m_Name);
    }
    virtual void Finalize()
    {
        BOOST_TEST_MESSAGE("finalize " << m_Name);
    }
    virtual void Reset()
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