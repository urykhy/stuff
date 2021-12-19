#pragma once

#include <llhttp.h>

#include <map>
#include <string>

#include <exception/Error.hpp>

namespace httpd {

    struct iless
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            return strcasecmp(a.data(), b.data()) < 0;
        }
    };
    // multi map ?
    using Headers = std::map<std::string, std::string, iless>;

    struct Request
    {
        using Handler = std::function<void(Request&)>;

        const char* method = nullptr;
        std::string url;
        Headers     headers;
        std::string body;
        bool        keep_alive{false};

        void clear()
        {
            method = nullptr;
            url.clear();
            headers.clear();
            body.clear();
            keep_alive = false;
        }
    };

    struct Response
    {
        using Handler = std::function<void(Response&)>;

        uint16_t    status = 0;
        Headers     headers;
        std::string body;
        bool        keep_alive{false};

        void clear()
        {
            status = 0;
            headers.clear();
            body.clear();
            keep_alive = false;
        }
    };

    template <class X>
    class Parser
    {
        llhttp_t          m_Parser;
        llhttp_settings_t m_Settings;

        static Parser* Get(llhttp_t* aParser) { return ((Parser*)aParser->data); }
        static int     on_message_begin(llhttp_t* aParser) { return Get(aParser)->on_message_begin_int(); }
        int            on_message_begin_int() { return 0; }

        static int on_url(llhttp_t* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_url_int(aData, aSize); }
        int        on_url_int(const char* aData, size_t aSize)
        {
            m_Data.url.append(aData, aSize);
            return 0;
        }

        static int on_header_field(llhttp_t* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_header_field_int(aData, aSize); }
        int        on_header_field_int(const char* aData, size_t aSize)
        {
            m_HeaderName.append(aData, aSize);
            return 0;
        }

        static int on_header_value(llhttp_t* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_header_value_int(aData, aSize); }
        int        on_header_value_int(const char* aData, size_t aSize)
        {
            m_HeaderValue.append(aData, aSize);
            return 0;
        }

        static int on_header_value_complete(llhttp_t* aParser) { return Get(aParser)->on_header_value_complete_int(); }
        int        on_header_value_complete_int()
        {
            m_Data.headers.insert(std::make_pair(std::move(m_HeaderName), std::move(m_HeaderValue)));
            m_HeaderName.clear();
            m_HeaderValue.clear();
            return 0;
        }

        static int on_body(llhttp_t* aParser, const char* aData, size_t aSize) { return Get(aParser)->on_body_int(aData, aSize); }
        int        on_body_int(const char* aData, size_t aSize)
        {
            m_Data.body.append(aData, aSize);
            return 0;
        }

        static int on_message_complete(llhttp_t* aParser) { return Get(aParser)->on_message_complete_int(); }
        int        on_message_complete_int()
        {
            m_Data.keep_alive = llhttp_should_keep_alive(&m_Parser);
            if constexpr (std::is_same_v<X, Request>)
                m_Data.method = llhttp_method_name((llhttp_method_t)m_Parser.method);
            else
                m_Data.status = m_Parser.status_code;
            m_Handler(m_Data);
            m_Data.clear();
            return 0;
        }

        typename X::Handler m_Handler;
        X                   m_Data;
        std::string         m_HeaderName;
        std::string         m_HeaderValue;

        void init2()
        {
            m_Parser.data = this;
            llhttp_settings_init(&m_Settings);
            m_Settings.on_message_begin = on_message_begin;
            if constexpr (std::is_same_v<X, Request>)
                m_Settings.on_url = on_url;
            m_Settings.on_header_field          = on_header_field;
            m_Settings.on_header_value          = on_header_value;
            m_Settings.on_header_value_complete = on_header_value_complete;
            m_Settings.on_body                  = on_body;
            m_Settings.on_message_complete      = on_message_complete;
        }

    public:
        using Error = Exception::Error<Parser>;

        Parser(typename X::Handler aHandler)
        : m_Handler(aHandler)
        {
            if constexpr (std::is_same_v<X, Request>) {
                llhttp_init(&m_Parser, HTTP_REQUEST, &m_Settings);
            } else {
                llhttp_init(&m_Parser, HTTP_RESPONSE, &m_Settings);
            }
            init2();
        }

        size_t consume(const char* aData, size_t aSize)
        {
            size_t sResult = llhttp_execute(&m_Parser, aData, aSize);
            if (sResult == HPE_PAUSED_H2_UPGRADE) {
                if constexpr (std::is_same_v<X, Request>) {
                    m_Data.method = llhttp_method_name((llhttp_method_t)m_Parser.method);
                    m_Data.body   = std::to_string(m_Parser.http_major) + '.' + std::to_string(m_Parser.http_minor);
                    m_Handler(m_Data);
                    return llhttp_get_error_pos(&m_Parser) - aData;
                } else {
                    throw Error(std::string("parser error: ") + llhttp_get_error_reason(&m_Parser));
                }
            } else if (sResult != HPE_OK)
                throw Error(std::string("parser error: ") + llhttp_get_error_reason(&m_Parser));
            return aSize;
        }
    };
} // namespace httpd
