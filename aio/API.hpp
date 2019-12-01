#pragma once

#include <aio.h>
#include <linux/aio_abi.h>
#include <sys/syscall.h>

extern "C"
{
    static inline long io_setup(unsigned nr_reqs, aio_context_t *ctx)   { return syscall(__NR_io_setup, nr_reqs, ctx); }
    static inline long io_destroy(aio_context_t ctx)                    { return syscall(__NR_io_destroy, ctx); }
    static inline long io_submit(aio_context_t ctx, long n, struct iocb **paiocb) { return syscall(__NR_io_submit, ctx, n, paiocb); }
    static inline long io_cancel(aio_context_t ctx, struct iocb *aiocb, struct io_event *res) { return syscall(__NR_io_cancel, ctx, aiocb, res); }
    static inline long io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events, struct timespec *tmo) { return syscall(__NR_io_getevents, ctx, min_nr, nr, events, tmo); }
}