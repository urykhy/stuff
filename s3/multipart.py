#!/usr/bin/env python3

import os

import boto3

FILENAME = "multipart.txt"
BUCKET = "test"

# get S3
session = boto3.Session(
    aws_access_key_id=os.environ["S3_ACCESS_KEY"], aws_secret_access_key=os.environ["S3_SECRET_KEY"]
)
s3 = session.client("s3", endpoint_url="http://" + os.environ["S3_HOST"])


# copy paste from https://stackoverflow.com/questions/61902684/boto3-possible-upload-generator-object-to-s3
class MultipartUpload:
    def __init__(self, client, bucket, key):
        self.client = client
        self.bucket = bucket
        self.key = key
        upload = self.client.create_multipart_upload(Bucket=bucket, Key=key)
        self.upload_id = upload["UploadId"]
        self.part = 1
        self.parts = []
        self.buffer = b""

    def write(self, content):
        if isinstance(content, str):
            self.buffer += content.encode("utf8")
        elif isinstance(content, bytes):
            self.buffer += content
        else:
            raise TypeError(f"MultipartUpload: Received bad data of type {type(content)}. Must be bytes or string")
        if len(self.buffer) > 5 * 1024 * 1024:  # 5 MiB Minimum part upload
            self.flush()

    def flush(self):
        resp = self.client.upload_part(
            Body=self.buffer, Bucket=self.bucket, Key=self.key, PartNumber=self.part, UploadId=self.upload_id
        )
        self.parts.append({"ETag": resp["ETag"], "PartNumber": self.part})
        self.buffer = b""
        self.part += 1

    def finish(self):
        if self.buffer:
            self.flush()
        return self.client.complete_multipart_upload(
            Bucket=self.bucket, Key=self.key, UploadId=self.upload_id, MultipartUpload={"Parts": self.parts}
        )

    def abort(self):
        self.client.abort_multipart_upload(Bucket=self.bucket, Key=self.key, UploadId=self.upload_id)

    def __enter__(self):
        return self

    def __exit__(self, type_, value, tb):
        if type_:
            self.abort()
        else:
            self.finish()


with MultipartUpload(s3, BUCKET, FILENAME) as upload:
    for line in ["one", "foo", "bar"]:
        print(line)
        upload.write(line + "\n")
