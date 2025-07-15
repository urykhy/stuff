#pragma once

#include <grpc/grpc.h>
#include <grpcpp/client_context.h>
#include <grpcpp/completion_queue.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server_builder.h>

#include "Play.grpc.pb.h"

#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <threads/Asio.hpp>
#include <unsorted/Raii.hpp>

namespace PlayGRPC {

    class Server : public play::PlayService::Service
    {
        std::unique_ptr<grpc::Server> m_Server;

    public:
        grpc::Status Ping(::grpc::ServerContext* aContext, const ::play::PingRequest* aRequest, ::play::PingResponse* aResponse) override
        {
            aResponse->set_value(aRequest->value());
            return grpc::Status::OK;
        }

        void Start(const std::string& aAddr)
        {
            grpc::ServerBuilder sBuilder;
            sBuilder.AddListeningPort(aAddr, grpc::InsecureServerCredentials());
            sBuilder.RegisterService(this);
            m_Server = sBuilder.BuildAndStart();
        }

        void Stop()
        {
            m_Server->Shutdown();
        }

        virtual ~Server() { Stop(); }
    };

    class AsyncServer
    {
        boost::asio::io_service*                     m_Asio;
        play::PlayService::AsyncService              m_Service;
        std::unique_ptr<grpc::Server>                m_Server;
        std::unique_ptr<grpc::ServerCompletionQueue> m_Queue;
        std::jthread                                 m_Thread;

        struct CallData
        {
            boost::asio::io_service*         m_Asio;
            play::PlayService::AsyncService* m_Service;
            grpc::ServerCompletionQueue*     m_Queue;
            grpc::ServerContext              m_Ctx;
            play::PingRequest                m_Request;
            play::PingResponse               m_Response;

            grpc::ServerAsyncResponseWriter<play::PingResponse> m_Responder;
            enum CallStatus
            {
                CREATE,
                PROCESS,
                FINISH
            };
            CallStatus m_Status;

            CallData(boost::asio::io_service* aAsio, play::PlayService::AsyncService* aService, grpc::ServerCompletionQueue* aQueue)
            : m_Asio(aAsio)
            , m_Service(aService)
            , m_Queue(aQueue)
            , m_Responder(&m_Ctx)
            , m_Status(CREATE)
            {
                Process();
            }

            boost::asio::awaitable<void> DoPing()
            {
                m_Response.set_value(m_Request.value());
                co_return;
            }

            void Process()
            {
                if (m_Status == CREATE) {
                    m_Status = PROCESS;
                    m_Service->RequestPing(&m_Ctx, &m_Request, &m_Responder, m_Queue, m_Queue, this);
                } else if (m_Status == PROCESS) {
                    new CallData(m_Asio, m_Service, m_Queue);

                    boost::asio::co_spawn(
                        *m_Asio,
                        [this]() mutable -> boost::asio::awaitable<void> {
                            co_await DoPing();
                            m_Status = FINISH;
                            m_Responder.Finish(m_Response, grpc::Status::OK, this);
                            co_return;
                        },
                        boost::asio::detached);

                } else {
                    delete this;
                }
            };
        };

        //  Can be run in multiple threads
        void ServeQueue()
        {
            new CallData(m_Asio, &m_Service, m_Queue.get());
            void* sTag;
            bool  sOk;
            while (m_Queue->Next(&sTag, &sOk) && sOk) {
                static_cast<CallData*>(sTag)->Process();
            }
        }

    public:
        AsyncServer(boost::asio::io_service& aAsio)
        : m_Asio(&aAsio)
        {
        }

        void Start(const std::string& aAddr)
        {
            grpc::ServerBuilder sBuilder;
            sBuilder.AddListeningPort(aAddr, grpc::InsecureServerCredentials());
            sBuilder.RegisterService(&m_Service);
            m_Queue  = sBuilder.AddCompletionQueue();
            m_Server = sBuilder.BuildAndStart();
            m_Thread = std::jthread([this] { ServeQueue(); });
        }

        void Stop()
        {
            m_Server->Shutdown();
            m_Queue->Shutdown();
        }

        virtual ~AsyncServer() { Stop(); }
    };

    class Client
    {
        std::unique_ptr<play::PlayService::Stub> m_Stub;

    public:
        Client(const std::string& aAddr)
        {
            auto sChannel = grpc::CreateChannel(aAddr, grpc::InsecureChannelCredentials());
            m_Stub        = play::PlayService::NewStub(sChannel);
        }

        int Ping(int aValue)
        {
            play::PingRequest  sRequest;
            play::PingResponse sResponse;
            sRequest.set_value(aValue);

            grpc::ClientContext sContext;
            grpc::Status        sStatus = m_Stub->Ping(&sContext, sRequest, &sResponse);
            if (!sStatus.ok()) {
                throw std::runtime_error("grpc call failed");
            }
            return sResponse.value();
        }

        virtual ~Client() = default;
    };

} // namespace PlayGRPC
