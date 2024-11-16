#!/usr/bin/env python3

import time

from confluent_kafka import Producer

SERVER = "broker-1.kafka.docker:9092"
TOPIC = "py_test1"

producer = Producer({"bootstrap.servers": SERVER})
start_ts = time.time()
for _ in range(10000):
    producer.produce(TOPIC, b"some_message_bytes")
producer.flush()
print(f"send 10000 messages in {time.time() - start_ts} seconds")
