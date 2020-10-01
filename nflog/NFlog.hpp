#pragma once

#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

extern "C"
{
#include <libnetfilter_log/libnetfilter_log.h>
}

#include <unsorted/Log4cxx.hpp>
#include <unsorted/Raii.hpp>

namespace NFlog {
    using Error = std::runtime_error;

    class Manager
    {
        nflog_handle*   m_Handle = nullptr;
        nflog_g_handle* m_Group  = nullptr;
        int             m_Fd     = -1;

        using Handler = std::function<void(struct nflog_data*)>;
        Handler m_Handler;

        static int
        callback(struct nflog_g_handle* gh, struct nfgenmsg* nfmsg, struct nflog_data* nfa, void* data)
        {
            try {
                ((Manager*)data)->m_Handler(nfa);
            } catch (const std::exception& e) {
                ERROR("fail to handle: " << e.what());
            }
            return 0;
        }

        void close()
        {
            if (m_Fd != -1) {
                ::close(m_Fd);
                m_Fd = -1;
            }
            if (m_Group != nullptr) {
                nflog_unbind_group(m_Group);
                m_Group = nullptr;
            }
            if (m_Handle != nullptr) {
                nflog_close(m_Handle);
                m_Handle = nullptr;
            }
        }

        void process_packet()
        {
            char sBuf[MAX_CAPLEN];
            memset(sBuf, 0, MAX_CAPLEN);
            int rc = recv(m_Fd, sBuf, MAX_CAPLEN, 0);
            if (rc > 0) {
                nflog_handle_packet(m_Handle, sBuf, rc);
            } else {
                ERROR("recv failed");
            }
        }

    public:
        enum
        {
            MAX_CAPLEN = 4096
        };

        Manager(int aGroup, Handler aHandler)
        : m_Handler(aHandler)
        {
            Util::Raii sCleanup([this]() {
                close();
            });

            m_Handle = nflog_open();
            if (m_Handle == nullptr) {
                throw Error("nflog_open");
            }

            DEBUG("binding socket to AF_INET");
            if (nflog_bind_pf(m_Handle, AF_INET) < 0) {
                throw Error("nflog_bind_pf");
            }

            DEBUG("binding socket to group");
            m_Group = nflog_bind_group(m_Handle, aGroup);
            if (m_Group == nullptr) {
                throw Error("nflog_bind_group");
            }
            nflog_callback_register(m_Group, &callback, this);

            DEBUG("set copy_packet mode");
            if (nflog_set_mode(m_Group, NFULNL_COPY_PACKET, MAX_CAPLEN) < 0) {
                throw Error("nflog_set_mode");
            }

            m_Fd = nflog_fd(m_Handle);
            // set non blocking ?
            sCleanup.dismiss();
        }

        ~Manager()
        {
            close();
        }

        void loop(volatile sig_atomic_t& aTerm)
        {
            struct pollfd sPoll;
            sPoll.fd = m_Fd;

            while (!aTerm) {
                sPoll.events  = POLLIN;
                sPoll.revents = 0;
                int rc        = poll(&sPoll, 1, 1000);
                if (rc == 1) {
                    if (sPoll.revents == POLLIN) {
                        process_packet();
                    } else {
                        FATAL("unexpected poll event: " << sPoll.revents << ", terminating");
                        aTerm = 1;
                    }
                }
            }
        }
    };
} // namespace Nflog