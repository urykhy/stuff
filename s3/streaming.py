#!/usr/bin/env python3

import gzip
import logging
import os
from io import TextIOWrapper

import boto3
import requests

FILENAME = "click.csv.gz"

# get stream from click
params = {
    "query": "select x_timestamp, query, uuid, message from vector where query != '' order by x_timestamp desc limit 10 format CSVWithNames",
    "enable_http_compression": "1",
}
headers = {"Accept-Encoding": "gzip"}
r = requests.get("http://master.clickhouse:8123/", params=params, headers=headers, stream=True)
r.raise_for_status()

# get S3
session = boto3.Session(
    aws_access_key_id=os.environ["S3_ACCESS_KEY"], aws_secret_access_key=os.environ["S3_SECRET_KEY"]
)
s3 = session.resource("s3", endpoint_url="http://" + os.environ["S3_HOST"])
bucket = s3.Bucket("test")
# print ([x.key for x in list (bucket.objects.all())])
boto3.set_stream_logger("", logging.DEBUG)

# stream to S3
bucket.upload_fileobj(r.raw, FILENAME)

# stream from S3
body = bucket.Object(key=FILENAME).get()["Body"]
for line in TextIOWrapper(gzip.GzipFile(fileobj=body, mode="rb")):
    print(line)
