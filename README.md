# stuff

some information about stuff

### large projects:
- jaeger - tracing events from c++
- sentry - sentry logs from c++, __cxa_throw hook.
- tnt17 - tarantool client with asio

### libraries
- curl - easy and multi handle c++ wrappers
- httpd - simple httpd server/client with pipelining
- mysql - mysql client
- threads - thread group, work queue, pipeline.

### experiments:
- af_alg - use AF_ALG socket on linux to avoid openssl.
- aio - linux native async io
- asio_http - asio http server and client
- asio_mysqld - asio mysql server
- avx - bit of simd
- bloom - bloom filters
- cache - some caching
- cbor - cbor format implementation
- container - containers, IoC.
- etcd - etcd v3 (json) client
- file - file operations
- httpd - httpd with pipelining and no asio
- iconv - c++ wrapper for iconv
- logger - logger which can be enabled/disabled per function/log-level
- ml - machine learning: FTRL, LR.
- mpl - some template magic
- msgpack - incomplete msgpack serialization
- mq - message queue over http, etcd used to avoid duplicates.
- networking - epoll based networking
- protobuf - fast protobuf parser with polymorphic allocator support
- prometheus - small client library
- resource - embed binary data into elf, tar file for example.
- rpc - simple rpc over udp
- ssl - openssl
- time - cctz wrapper, time helpers
- utf8 - utf8 validator

### scripts
- btsync  - transfer files over bittorrent
- esalert - check logs in elasticsearch
- samlib  - to make fb2 ebook from samlib
- scripts - unsorted, iptables, qos, make deb packages, make certificates and so on.
- testing/ctest-allure.py - generate xml to show ctest results in allure.
- testing/coverage-allure.py - generate xml to show coverage results in allure.

### web api
- flibusta/ - interface to search books over local flibusta library
- torrent/ - interface to use rutracker database
