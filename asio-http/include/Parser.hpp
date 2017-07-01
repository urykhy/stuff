
#ifndef _HTTP_PARSER_HPP__
#define _HTTP_PARSER_HPP__

#include <iostream>
#include <list>
#include <string>
#include <cassert>
#include <ext/pool_allocator.h>
#include <functional>

#include <http_parser.h>

#define _PUBLIC  __attribute__ ((visibility ("default")))
#define _PRIVATE  __attribute__ ((visibility ("hidden")))

/*
 * test magic:
 *      objdump -tTC lib/libstuff.so  | grep Parser
 * -> internals are in local symbol table
 * -> only c-tor + operator() are exported
 */

namespace HTTP {

    typedef std::pair<std::string, std::string> HeaderValue;
    typedef std::list<HeaderValue, __gnu_cxx::__pool_alloc<char>> HeaderList;

    class Parser
    {
    public:
        typedef std::function<int (int, Parser*)> Handler;

    protected:
        _PRIVATE static http_parser_settings current;
        _PRIVATE static int cb_message_begin(http_parser*);
        _PRIVATE static int cb_url(http_parser* p, const char *at, size_t length);
        _PRIVATE static int cb_status(http_parser* p, const char *at, size_t length);
        _PRIVATE static int cb_header_field(http_parser* p, const char *at, size_t length);
        _PRIVATE static int cb_header_value(http_parser* p, const char *at, size_t length);
        _PRIVATE static int cb_headers_complete(http_parser*);
        _PRIVATE static int cb_body(http_parser* p, const char *at, size_t length);
        _PRIVATE static int cb_message_complete(http_parser* p);

        _PRIVATE virtual int on_url(const char *at, size_t length);
        _PRIVATE virtual int on_header_field(const char *at, size_t length);
        _PRIVATE virtual int on_header_value(const char *at, size_t length);
        _PRIVATE virtual int on_body(const char *at, size_t length);
        _PRIVATE virtual int headers_complete();
        _PRIVATE virtual int message_complete() = 0;

        // data
        http_parser parser;
        bool new_header = true;
        bool done = false;
        const char* last_err = 0;

    public:
        Parser(enum http_parser_type type);
        bool operator()(const char *data, size_t len);
        virtual void clear() = 0;
        bool is_done() const;
        const char* last_error() const {return last_err;}

        // common data
        HeaderList headers;
    };

    struct RequestParser : public Parser
    {
        RequestParser();
        virtual void clear();

        std::string url;
        int method = 0;

    protected:
        //Handler user;
        _PRIVATE virtual int on_url(const char *at, size_t length) override;
        _PRIVATE virtual int message_complete() override;
    };

    struct ResponseParser : public Parser
    {
        ResponseParser();
        virtual void clear();

        std::string data;
        int code = 0;
        bool keep_alive = false;

    protected:
        //Handler user;
        _PRIVATE virtual int on_body(const char *at, size_t length) override;
        _PRIVATE virtual int message_complete() override;
    };

} // namespace Util

#endif /* _HTTP_PARSER_HPP__ */

