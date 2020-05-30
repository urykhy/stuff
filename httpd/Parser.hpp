#pragma once

#include <string>
#include <vector>

#include <exception/Error.hpp>

#include "http_parser.h"

namespace httpd {

    struct RRCommon
    {
        struct Header
        {
            std::string key;
            std::string value;
        };
        using Headers = std::vector<Header>;

        Headers     m_Headers;
        std::string m_Body;
        bool        m_KeepAlive{false};

        void clear()
        {
            m_Headers.clear();
            m_Body.clear();
            m_KeepAlive = false;
        }
    };

    struct Request : public RRCommon
    {
        using Handler = std::function<void(Request&)>;

        const char* m_Method = nullptr;
        std::string m_Url;

        void clear()
        {
            m_Method = nullptr;
            m_Url.clear();
            RRCommon::clear();
        }
    };

    struct Response : public RRCommon
    {
        using Handler = std::function<void(Response&)>;

        uint16_t m_Status = 0;

        void clear()
        {
            m_Status = 0;
            RRCommon::clear();
        }
    };

    class Parser
    {
        struct http_parser          m_Parser;
        struct http_parser_settings m_Settings;

        static Parser* Get(http_parser* aParser) { return ((Parser*)aParser->data); }
        static int     on_message_begin(http_parser* aParser) { return Get(aParser)->on_message_begin_int(); }
        int            on_message_begin_int() { return 0; }

        static int on_url(http_parser* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_url_int(aData, aSize); }
        int        on_url_int(const char* aData, size_t aSize)
        {
            if (!m_ModeRequest)
                return -1;

            m_Request.m_Url.append(aData, aSize);
            return 0;
        }

        static int on_header_field(http_parser* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_header_field_int(aData, aSize); }
        int        on_header_field_int(const char* aData, size_t aSize)
        {
            if (m_InsertHeader) {
                m_Common->m_Headers.push_back(Request::Header{});
                m_InsertHeader = false;
            }
            m_Common->m_Headers.back().key.append(aData, aSize);
            return 0;
        }

        static int on_header_value(http_parser* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_header_value_int(aData, aSize); }
        int        on_header_value_int(const char* aData, size_t aSize)
        {
            m_InsertHeader = true;
            m_Common->m_Headers.back().value.append(aData, aSize);
            return 0;
        }

        static int on_body(http_parser* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_body_int(aData, aSize); }
        int        on_body_int(const char* aData, size_t aSize)
        {
            m_Common->m_Body.append(aData, aSize);
            return 0;
        }

        static int on_message_complete(http_parser* aParser) { return Get(aParser)->on_message_complete_int(); }
        int        on_message_complete_int()
        {
            m_Common->m_KeepAlive = http_should_keep_alive(&m_Parser);
            if (m_ModeRequest) {
                m_Request.m_Method = http_method_str((http_method)m_Parser.method);
                m_RequestHandler(m_Request);
            } else {
                m_Response.m_Status = m_Parser.status_code;
                m_ResponseHandler(m_Response);
            }
            clear();
            return 0;
        }

        void clear()
        {
            m_Request.clear();
            m_Response.clear();
            m_InsertHeader = true;
        }

        const bool        m_ModeRequest;
        Request::Handler  m_RequestHandler;
        Response::Handler m_ResponseHandler;

        Request   m_Request;
        Response  m_Response;
        RRCommon* m_Common = nullptr;
        bool      m_InsertHeader = true;

        void init2()
        {
            m_Parser.data = this;

            http_parser_settings_init(&m_Settings);
            m_Settings.on_message_begin    = on_message_begin;
            m_Settings.on_url              = on_url;
            m_Settings.on_header_field     = on_header_field;
            m_Settings.on_header_value     = on_header_value;
            m_Settings.on_body             = on_body;
            m_Settings.on_message_complete = on_message_complete;
        }

    public:
        using Error = Exception::Error<Parser>;

        Parser(Request::Handler aHandler)
        : m_ModeRequest(true)
        , m_RequestHandler(aHandler)
        , m_Common(&m_Request)
        {
            http_parser_init(&m_Parser, HTTP_REQUEST);
            init2();
        }

        Parser(Response::Handler aHandler)
        : m_ModeRequest(false)
        , m_ResponseHandler(aHandler)
        , m_Common(&m_Response)
        {
            http_parser_init(&m_Parser, HTTP_RESPONSE);
            init2();
        }

        size_t consume(const char* aData, size_t aSize)
        {
            size_t sUsed = http_parser_execute(&m_Parser, &m_Settings, aData, aSize);
            if (m_Parser.http_errno)
                throw Error(std::string("parser error: ") + http_errno_description(HTTP_PARSER_ERRNO(&m_Parser)));
            return sUsed;
        }
    };
} // namespace httpd