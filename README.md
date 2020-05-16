# stuff

some information about stuff

### large projects:
- jaeger - tracing events from c++
- sentry - sentry logs from c++, __cxa_throw hook.
- tnt17 - tarantool client with asio

### libraries
- curl - easy and multi handle c++ wrappers
- httpd - simple httpd server
- mysql - mysql client
- threads - thread group, work queue, pipeline.

### experiments:
- af_alg - use AF_ALG socket on linux to avoid openssl.
- aio - linux native async io
- asio_http - asio http server and client
- asio_mysqld - asio mysql server
- avx - bit of simd
- cache - some caching
- cbor - cbor format implementation
- container - containers, IoC.
- file - file operations
- httpd - httpd with pipelining and no asio
- iconv - c++ wrapper for iconv
- logger - logger which can be enabled/disabled per function/log-level
- ml - machine learning: FTRL, LR.
- mpl - some template magic
- msgpack - incomplete msgpack serialization
- networking - epoll based networking
- protobuf - fast protobuf parser with polymorphic allocator support
- resource - embed binary data into elf, tar file for example.
- rpc - simple rpc over udp
- ssl - openssl wrapper
- stats - graphite/prometheus metrics proof of concept
- time - cctz wrapper
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
