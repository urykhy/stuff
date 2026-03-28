#!/usr/bin/env python3

import asyncio
import logging
import sys
import time

sys.path.append(".")

import Play_pb2
import Play_pb2_grpc

import grpc


async def load(limit) -> None:
    async with grpc.aio.insecure_channel("127.0.0.1:56780") as channel:
        stub = Play_pb2_grpc.PlayServiceStub(channel)
        count = 0
        while count < limit:
            await stub.Ping(Play_pb2.PingRequest(value=count))
            count += 1


async def run() -> None:
    REQUESTS = 60000
    COROUTINES = 10
    start_at = time.perf_counter()
    tasks = [load(REQUESTS / COROUTINES) for i in range(COROUTINES)]
    results = await asyncio.gather(*tasks)
    elapsed = time.perf_counter() - start_at
    print(
        f"Make {REQUESTS} requests in {elapsed:0.1f}s using {COROUTINES} coroutines. avg RPS: {REQUESTS/elapsed:0.0f}"
    )


if __name__ == "__main__":
    logging.basicConfig()
    asyncio.run(run())
