#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <sys/uio.h>
#include <liburing.h>

#include <unsorted/Raii.hpp>
#include <threads/Group.hpp>
#include <exception/Error.hpp>

namespace Util
{
    struct URingUser
    {
        using Ptr = std::shared_ptr<URingUser>;
        virtual void on_event(int aKind, int32_t aRes) = 0;
        virtual ~URingUser() {};
    };

    struct URing
    {
        using Ptr = URingUser::Ptr;

        using Error = Exception::Error<URing>;

    private:
        const size_t m_Depth;
        std::atomic_bool m_Running{true};

        struct Task
        {
            int op{IORING_OP_NOP};
            int fd = -1;
            Ptr user;
            struct iovec buffer{};
            struct sockaddr* addr = nullptr;
            socklen_t* addrlen = nullptr;
            struct __kernel_timespec timeout{0, 10 * 1000 * 1000 /* 10 ms */};
        };

        using Lock = std::unique_lock<std::mutex>;
        std::mutex m_Mutex;
        std::vector<Task> m_Tasks;

        struct io_uring m_Ring;
        uint64_t m_Serial{1};   // start from 1, 0 used as no-callback hint
        std::map<uint64_t, Task> m_ActiveTasks;
        std::vector<struct io_uring_cqe*> m_CQE;

        struct Wakeup : public URingUser
        {   // just a place holder
            void on_event(int aKind, int32_t aRes) override { ;; }
        };

        void prepare()
        {
            std::vector<Task> sTasks;
            // limit number of entries in queue
            // check io_uring_get_sqe return not null
            {
                Lock lk(m_Mutex);
                sTasks = std::move(m_Tasks);
                m_Tasks.reserve(m_Depth);
            }
            for (auto& x : sTasks)
            {
                uint64_t sSerial = m_Serial++;
                m_ActiveTasks.emplace(sSerial, x);
                struct io_uring_sqe* sSQE = io_uring_get_sqe(&m_Ring);
                switch (x.op)
                {
                    case IORING_OP_ACCEPT:  //BOOST_TEST_MESSAGE("accept on " << x.fd);
                                            io_uring_prep_accept (sSQE, x.fd, 0, 0, 0);
                                            break;
                    case IORING_OP_CONNECT: {
                                            //BOOST_TEST_MESSAGE("connect on " << x.fd);
                                            io_uring_prep_connect(sSQE, x.fd, x.addr, *x.addrlen);
                                            sSQE->flags |= IOSQE_IO_LINK; // forms a link with the next SQE
                                            struct io_uring_sqe* sTimeout = io_uring_get_sqe(&m_Ring);
                                            // FIXME: if we got 0 - we can't place connect into queue. we mus ensure queue size is large enough before operation
                                            io_uring_prep_link_timeout(sTimeout, &x.timeout, 0);
                                            io_uring_sqe_set_data(sTimeout, (void*)0);
                                            break;
                    }
                    case IORING_OP_READ:    //BOOST_TEST_MESSAGE("read on " << x.fd << " into buffer with " << x.buffer.iov_len << " bytes");
                                            io_uring_prep_read   (sSQE, x.fd, x.buffer.iov_base, x.buffer.iov_len, 0);
                                            break;
                    case IORING_OP_WRITE:   //BOOST_TEST_MESSAGE("write on " << x.fd << " from buffer of " << x.buffer.iov_len << " bytes");
                                            io_uring_prep_write  (sSQE, x.fd, x.buffer.iov_base, x.buffer.iov_len, 0);
                                            break;
                    case IORING_OP_CLOSE:   //BOOST_TEST_MESSAGE("close on " << x.fd);
                                            io_uring_prep_close  (sSQE, x.fd);
                                            break;
                    default: assert(0);
                }
                io_uring_sqe_set_data(sSQE, (void*)sSerial);
            }
        }

        void process(struct io_uring_cqe *aCQE)
        {
            Util::Raii sCleanup1([this, aCQE](){ io_uring_cqe_seen(&m_Ring, aCQE); });

            const uint64_t sIndex = (uint64_t)io_uring_cqe_get_data(aCQE);
            const auto sIt = m_ActiveTasks.find(sIndex);
            if (sIt == m_ActiveTasks.end())
                return;
            Util::Raii sCleanup2([this, sIt](){ m_ActiveTasks.erase(sIt); });

            sIt->second.user->on_event(sIt->second.op, aCQE->res);
        }

    public:

        URing(size_t aDepth = 1024)
        : m_Depth(aDepth)
        {
            m_Tasks.reserve(aDepth);
            m_CQE.resize(aDepth);
            int rc = io_uring_queue_init(aDepth, &m_Ring, 0);
            if (rc)
                throw Error("fail to init uring: " + std::to_string(rc));
        }

        ~URing()
        {
            io_uring_queue_exit(&m_Ring);
        }

        void accept(int aFD, Ptr aPtr)
        {
            Lock lk(m_Mutex);
            m_Tasks.push_back(Task{IORING_OP_ACCEPT, aFD, aPtr});
            //wakeup via event fd
        }

        // add connect timeout
        void connect(int aFD, Ptr aPtr, struct sockaddr* addr, socklen_t* addrlen)
        {
            Lock lk(m_Mutex);
            m_Tasks.push_back(Task{IORING_OP_CONNECT, aFD, aPtr, {}, addr, addrlen});
        }

        void read(int aFD, Ptr aPtr, struct iovec aBuffer)
        {
            Lock lk(m_Mutex);
            m_Tasks.push_back(Task{IORING_OP_READ, aFD, aPtr, aBuffer});
        }

        void write(int aFD, Ptr aPtr, struct iovec aBuffer)
        {
            Lock lk(m_Mutex);
            m_Tasks.push_back(Task{IORING_OP_WRITE, aFD, aPtr, aBuffer});
        }

        void close(int aFD, Ptr aPtr)
        {
            Lock lk(m_Mutex);
            m_Tasks.push_back(Task{IORING_OP_CLOSE, aFD, aPtr});
        }

        void dispatch()
        {
            prepare();
            io_uring_submit(&m_Ring);

            struct __kernel_timespec sTimeout{0, 10 * 1000 * 1000 /* 10ms */ };
            struct io_uring_cqe* sCQE = nullptr;

            // io_uring_wait_cqe_timeout can return EAGAIN if queue is full since it equeue io_uring_prep_timeout operation
            if (0 == io_uring_wait_cqe_timeout(&m_Ring, &sCQE, &sTimeout))
            {
                process(sCQE);
                int sCount = io_uring_peek_batch_cqe(&m_Ring, m_CQE.data(), m_Depth);
                for (int i = 0; i < sCount; i++)
                    process(m_CQE[i]);
                std::fill(m_CQE.begin(), m_CQE.end(), nullptr);
            }
        }

        void start(Threads::Group& aGroup)
        {
            aGroup.start([this](){
                while (m_Running)
                    dispatch();
            });
            aGroup.at_stop([this](){
                m_Running = false;
            });
        }
    };
}