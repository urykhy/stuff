#!/usr/bin/env python3

import queue
import random
import string
import threading
import time

from confluent_kafka import Consumer, Producer

SERVER = "broker-1.kafka.docker:9092"
SRC_TOPIC = "py_test1"
DST_TOPIC = "py_test2"

consumer = Consumer(
    {
        "bootstrap.servers": SERVER,
        "auto.offset.reset": "earliest",
        "enable.auto.commit": False,
        "group.id": "test-" + "".join(random.choice(string.digits) for i in range(10)),
    }
)
consumer.subscribe([SRC_TOPIC])
producer = Producer({"bootstrap.servers": SERVER})

# prepare consumer
while True:
    msg = consumer.poll(timeout=1.0)
    if msg is not None and not msg.error():
        break
    print(f"wait for broker ...")


def worker():
    while True:
        msg = q.get()
        producer.produce(DST_TOPIC, str(msg).encode("UTF-8"))
        q.task_done()


q = queue.Queue()
threading.Thread(target=worker, daemon=True).start()

# processing
start_ts = time.time()
count = 0
MAX_COUNT = 10000
while count < MAX_COUNT:
    msg = consumer.poll(timeout=1.0)
    if msg is None or msg.error():
        continue
    q.put(msg)
    count += 1
q.join()
producer.flush()
print(f"processed {MAX_COUNT} messages in {(time.time() - start_ts):.3} seconds")
