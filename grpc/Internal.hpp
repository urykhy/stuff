
#pragma once

#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>

#include <mpl/Mpl.hpp>
#include <threads/Asio.hpp>
#include <threads/Group.hpp>

#define AGRPC_BOOST_ASIO
#include <agrpc/client_rpc.hpp>
#include <agrpc/grpc_context.hpp>
#include <agrpc/register_awaitable_rpc_handler.hpp>
#include <agrpc/server_rpc.hpp>

namespace GRPC::Internal {

    class Server
    {
        using ContextPtr = std::shared_ptr<agrpc::GrpcContext>;
        std::unique_ptr<grpc::Server> m_Server;
        grpc::ServerBuilder           m_Builder;
        std::vector<ContextPtr>       m_Contexts;
        Threads::Group                m_Thread;
        const unsigned                m_ThreadCount;

    public:
        template <class... T>
        Server(const std::string& aAddr, unsigned aThreadCount, T&... aServices)
        : m_ThreadCount(aThreadCount)
        {
            for (unsigned i = 0; i < m_ThreadCount; i++) {
                m_Contexts.push_back(std::make_shared<agrpc::GrpcContext>(m_Builder.AddCompletionQueue()));
            }

            Mpl::for_each_argument([this](auto& x) { x.Prepare(m_Builder, m_Contexts); }, aServices...);
            m_Builder.AddListeningPort(aAddr, grpc::InsecureServerCredentials());

            m_Server = m_Builder.BuildAndStart();
            for (auto& sContext : m_Contexts) {
                m_Thread.start([this, sContext]() { prctl(PR_SET_NAME, "grpc:server"); sContext->run(); });
            }
        }

        void Stop()
        {
            m_Server->Shutdown();
            m_Thread.wait();
        }

        virtual ~Server() { Stop(); }
    };

    class Client
    {
        using ContextPtr = std::shared_ptr<agrpc::GrpcContext>;
        std::vector<ContextPtr> m_Contexts;
        Threads::Group          m_Thread;
        const std::string       m_Addr;
        const unsigned          m_ThreadCount;
        std::atomic<unsigned>   m_Current{0};

    public:
        template <class... T>
        Client(const std::string& aAddr, unsigned aThreadCount, T&... aServices)
        : m_Addr(aAddr)
        , m_ThreadCount(aThreadCount)
        {
            for (unsigned i = 0; i < m_ThreadCount; i++) {
                m_Contexts.push_back(std::make_shared<agrpc::GrpcContext>());
            }

            Mpl::for_each_argument([this](auto& x) { x.Prepare(*this); }, aServices...);

            for (auto& sContext : m_Contexts) {
                m_Thread.start(
                    [this, sContext] {
                        prctl(PR_SET_NAME, "grpc:client");
                        boost::asio::executor_work_guard<decltype(sContext->get_executor())> sWork{sContext->get_executor()};
                        sContext->run();
                    });
            }
        }

        const std::string& Addr() const { return m_Addr; }

        ContextPtr Context()
        {
            const auto sCurrent = m_Current.fetch_add(1, std::memory_order_relaxed) % m_ThreadCount;
            return m_Contexts[sCurrent];
        };

        void Stop()
        {
            for (auto& sContext : m_Contexts) {
                sContext->stop();
            }
            m_Thread.wait();
        }

        ~Client()
        {
            Stop();
        }
    };

} // namespace GRPC::Internal
