#define BOOST_TEST_MODULE Suites

#include <boost/test/unit_test.hpp>

#include <set>

#include "Entity.hpp"

BOOST_AUTO_TEST_SUITE(ecs)
BOOST_AUTO_TEST_CASE(entity)
{
    struct C1
    {};
    struct C2
    {
        int m_Data = 0;
        C2(int aData)
        : m_Data(aData)
        {}
    };
    using E = ECS::Entity<C1, C2>;

    auto e = E::create();
    e.assign(C2{42});
    BOOST_CHECK_MESSAGE(e.get<C2>().value()->m_Data == 42, "get component value");
    e.erase<C2>();
    BOOST_CHECK_MESSAGE(!e.get<C2>(), "get nx component value");

    e.assign(C1{});
    bool sWithC1 = false;
    e.inspect(Mpl::overloaded{[&sWithC1](C1& x) mutable { sWithC1 = true; },
                              [](auto& x) { BOOST_CHECK_MESSAGE(false, "called with unexpected type " << typeid(x).name()); }});
    BOOST_CHECK_MESSAGE(sWithC1, "component1 attached");

    auto& sSystem  = E::system();
    int   sCountC1 = 0;
    sSystem.forEach<C1>([&sCountC1](auto&& aEntity, C1& aData) {
        BOOST_TEST_MESSAGE("entity " << aEntity.getID());
        BOOST_CHECK_MESSAGE(!aEntity.template get<C2>(), "get nx component value");
        sCountC1++;
    });
    BOOST_CHECK_MESSAGE(sCountC1 == 1, "number of C1 components");
}

namespace XTest {
    struct Origin
    {
        uint32_t    id = 0; // entity to object id
        std::string comment;
    };
    struct Updated
    {};
    struct Priority
    {};
    struct Links
    {
        std::set<uint32_t> set;
    };
    struct Map
    {
        using E    = ECS::Entity<Origin, Updated, Priority, Links>;
        using TE   = ECS::TmpEntity<Origin, Updated, Priority, Links>;
        using EPtr = std::shared_ptr<E>;

        std::map<uint32_t, EPtr> m_Map;

        void create(Origin&& aOrig)
        {
            auto sEntry     = std::make_shared<E>(E::create());
            m_Map[aOrig.id] = sEntry;
            sEntry->assign(std::move(aOrig));
        }
        void erase(uint32_t id) { m_Map.erase(id); }
        void update(uint32_t id)
        {
            m_Map[id]->assign(Updated{});
            touchLinks(id);
        }
        void priority(uint32_t id) { m_Map[id]->assign(Priority{}); } // ask for priority refresh

        bool stale(uint32_t id) { return m_Map[id]->get<Updated>().has_value(); }

        // T called to process refresh. must return true if all ok
        template <class K, class T>
        void refresh(T&& t)
        {
            std::list<uint64_t> sTmpList; // entity id list. collect since we can't remove component K while iterating over K
            E::system().forEach<K>([&sTmpList](auto&& aEntity, auto& aData) {
                sTmpList.push_back(aEntity.getID());
            });
            for (auto& x : sTmpList) {
                TE tmp(x);
                if (t(tmp.get<Origin>().value())) {
                    tmp.erase<Priority>();
                    tmp.erase<Updated>();
                }
            }
        }

        // FIXME: recursion
        void touchLinks(uint32_t id)
        {
            auto sEntity = m_Map[id];
            auto sLinks  = sEntity->get<Links>();
            if (!sLinks)
                return;
            for (auto x : sLinks.value()->set)
                update(x);
        }

        Links* getLinks(uint32_t id)
        {
            auto sEntity = m_Map[id];
            auto sLinks  = sEntity->get<Links>();
            if (!sLinks) {
                sEntity->assign(Links{});
                sLinks = sEntity->get<Links>();
            }
            return sLinks.value();
        }
        void link(uint32_t id, uint32_t ref)
        {
            if (id == ref)
                return;
            getLinks(id)->set.insert(ref);
        }
        void unlink(uint32_t id, uint32_t ref)
        {
            auto sLinksPtr = getLinks(id);
            sLinksPtr->set.erase(ref);
            if (sLinksPtr->set.empty())
                m_Map[id]->erase<Links>();
        }
    };
} // namespace XTest

BOOST_AUTO_TEST_CASE(xtest)
{
    XTest::Map sMap;
    sMap.create(XTest::Origin{10, "foo"});
    sMap.update(10);
    sMap.refresh<XTest::Updated>([](auto* origin) {
        BOOST_TEST_MESSAGE("process updated " << origin->id << "/" << origin->comment);
        return true;
    });
    sMap.priority(10);
    sMap.refresh<XTest::Priority>([](auto* origin) {
        BOOST_TEST_MESSAGE("process priority " << origin->id << "/" << origin->comment);
        return true;
    });
    BOOST_CHECK_EQUAL(sMap.stale(10), false);

    BOOST_TEST_MESSAGE("stage 2");
    // part 2. links
    sMap.create(XTest::Origin{11, "bar"});
    sMap.create(XTest::Origin{20, "d2: use d1"});
    sMap.create(XTest::Origin{30, "d1: use foo and bar"});
    sMap.link(10, 30);
    sMap.link(11, 30);
    sMap.link(30, 20);

    // update bar and process.
    sMap.update(11);
    BOOST_CHECK_EQUAL(sMap.stale(30), true);
    sMap.refresh<XTest::Updated>([](auto* origin) {
        // item must ensure if can be processed
        // actual refresh order - by entity.id asc
        BOOST_TEST_MESSAGE("process updated " << origin->id << "/" << origin->comment);
        return true;
    });
    BOOST_CHECK_EQUAL(sMap.stale(30), false);
}
BOOST_AUTO_TEST_SUITE_END()
