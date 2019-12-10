#pragma once

#include <atomic>

namespace tnt17
{
    enum StateEnum
    {
        IDLE
      , CONNECTING
      , TIMEOUT
      , AUTH
      , ALIVE
      , ERROR
    };

    class State
    {
        std::atomic_bool       m_Running{true};
        std::atomic<StateEnum> m_State{IDLE};
    public:

        void start() { m_Running = true; }
        void stop()  { m_Running = false; }
        bool is_running()   const { return m_Running; }

        void connecting()  { m_State = CONNECTING; }
        void timeout()     { m_State = TIMEOUT; }
        void auth()        { m_State = AUTH; }
        void established() { m_State = ALIVE; }
        void set_error()   { m_State = ERROR; }
        void close()       { m_State = IDLE; }
        bool is_connected() const { return m_State == ALIVE; }
        StateEnum state()   const { return m_State; }

        bool is_alive() const { return is_running() and is_connected(); }
    };

} // namespace tnt17