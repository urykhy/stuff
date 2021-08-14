#pragma once

#include "Jaeger.hpp"

#include <unsorted/Log4cxx.hpp>

namespace Jaeger {

    struct Helper : boost::noncopyable
    {
        using Step    = Metric::Step;
        using StepPtr = std::unique_ptr<Step>;

    private:
        Params m_Params;
        bool   m_Active = false;

        std::unique_ptr<Metric> m_Metric;
        StepPtr                 m_Current;

    public:
        Helper(const std::string& aParent, int64_t aBaseId, std::string_view aService)
        {
            if (!aParent.empty()) {
                m_Params         = Params::parse(aParent);
                m_Params.baseId  = aBaseId;
                m_Params.service = aService;
                m_Metric         = std::make_unique<Metric>(m_Params);
                m_Active         = true;
            }
        }

        void start(const std::string& aStage)
        {
            if (!m_Active)
                return;
            stop();
            m_Current = std::make_unique<Step>(*m_Metric, aStage);
        }
        void stop()
        {
            if (m_Current)
                m_Current.reset();
        }
        void set_error(const char* aMessage)
        {
            if (m_Current) {
                m_Current->set_error();
                m_Current->set_log(Metric::Tag{"exception", aMessage});
            }
        }

        StepPtr child(const std::string& aName)
        {
            StepPtr sResult;
            if (m_Current) {
                sResult = std::make_unique<Step>(m_Current->child(aName));
            }
            return sResult;
        }

        static StepPtr child(StepPtr& aStep, const std::string& aName)
        {
            StepPtr sResult;
            if (aStep) {
                sResult = std::make_unique<Step>(aStep->child(aName));
            }
            return sResult;
        }

        ~Helper()
        {
            stop();
            if (m_Active)
                send(*m_Metric);
        }
    };
} // namespace Jaeger