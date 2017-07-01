
#include <Parser.hpp>

namespace HTTP {

    Parser::Parser(enum http_parser_type type)
    {
        http_parser_init(&parser, type);
        parser.data = this;
    }

    bool
    Parser::operator()(const char *data, size_t len)
    {
        http_parser_execute(&parser, &current, data, len);
        if (HTTP_PARSER_ERRNO(&parser)) {
            last_err = http_errno_description(HTTP_PARSER_ERRNO(&parser));
            return false;
        }
        return true;
    }

    int Parser::cb_message_begin(http_parser* p) {
        return 0;
    }
    int Parser::cb_url(http_parser* p, const char *at, size_t length) {
        return static_cast<Parser*>(p->data)->on_url(at, length);
    }
    int Parser::cb_status(http_parser* p, const char *at, size_t length) {
        return 0;
    }
    int Parser::cb_header_field(http_parser* p, const char *at, size_t length) {
        return static_cast<Parser*>(p->data)->on_header_field(at, length);
    }
    int Parser::cb_header_value(http_parser* p, const char *at, size_t length) {
        return static_cast<Parser*>(p->data)->on_header_value(at, length);
    }
    int Parser::cb_headers_complete(http_parser* p) {
        return static_cast<Parser*>(p->data)->headers_complete();
    }
    int Parser::cb_body(http_parser* p, const char *at, size_t length) {
        return static_cast<Parser*>(p->data)->on_body(at, length);
    }
    int Parser::cb_message_complete(http_parser* p) {
        return static_cast<Parser*>(p->data)->message_complete();
    }

    http_parser_settings Parser::current = {
        Parser::cb_message_begin,
        Parser::cb_url,
        Parser::cb_status,
        Parser::cb_header_field,
        Parser::cb_header_value,
        Parser::cb_headers_complete,
        Parser::cb_body,
        Parser::cb_message_complete
    };

    int Parser::on_url(const char *at, size_t length)
    {
        assert(0);
        return 0;
    }
    int Parser::on_body(const char *at, size_t length)
    {
        //assert(0);
        return 0;
    }

    int Parser::on_header_field(const char *at, size_t length)
    {
        if (new_header)
        {
            headers.push_back(HeaderValue());
        }
        headers.back().first.append(at, length);
        return 0;
    }
    int Parser::on_header_value(const char *at, size_t length)
    {
        headers.back().second.append(at, length);
        new_header = true;
        return 0;
    }
    int Parser::headers_complete()
    {
        return 0;
    }
    bool Parser::is_done() const {
        return done;
    }

    //

    RequestParser::RequestParser()
    : Parser(HTTP_REQUEST)
    {
        ;;
    }
    int RequestParser::on_url(const char *at, size_t length)
    {
        url.append(at, length);
        return 0;
    }
    int RequestParser::message_complete()
    {
        method = parser.method;
        done = true;
        return 0;
    }
    void RequestParser::clear()
    {
        headers.clear();
        new_header = true;
        done = false;
        last_err = 0;
        url.clear();
        method = 0;
        http_parser_init(&parser, HTTP_REQUEST);
    }

    //

    ResponseParser::ResponseParser()
    : Parser(HTTP_RESPONSE)
    {
        ;;
    }
    int ResponseParser::on_body(const char *at, size_t length)
    {
        data.append(at, length);
        return 0;
    }
    int ResponseParser::message_complete()
    {
        code = parser.status_code;
        keep_alive = http_should_keep_alive(&parser);
        done = true;
        return 0;
    }
    void ResponseParser::clear()
    {
        headers.clear();
        new_header = true;
        done = false;
        last_err = 0;
        data.clear();
        code = 0;
        keep_alive = 0;
        http_parser_init(&parser, HTTP_RESPONSE);
    }

}


