#pragma once

#include <memory>

#include "HPack.hpp"
#include "InputBuf.hpp"
#include "Types.hpp"

namespace asio_http::v2::parser {

    struct IMessage
    {
        virtual void set_header(std::string& aName, std::string& aValue) = 0;
        virtual void append_body(std::string& aBody)                     = 0;
        virtual ~IMessage() {}
    };
    using MessagePtr = std::shared_ptr<IMessage>;

    struct API
    {
        virtual void       established(){};
        virtual MessagePtr new_message(uint32_t aID)                            = 0;
        virtual void       process_message(uint32_t aID, MessagePtr&& aMessage) = 0;
        virtual void       window_update(uint32_t aID, uint32_t aInc)           = 0;
        virtual void       send(std::string&& aBuffer)                          = 0;
        virtual ~API() {}
    };

    enum Mode
    {
        CLIENT = 0,
        SERVER
    };

    struct Main
    {
        using Error = std::runtime_error;

    private:
        const Mode m_Mode;
        API*       m_API;

#ifdef BOOST_TEST_MODULE
    public:
#endif
        enum Stage
        {
            HELLO,
            HEADER,
            BODY,
            CLIENT_HELLO,
            CLIENT_WAIT
        };
        Stage m_Stage{HELLO};
#ifdef BOOST_TEST_MODULE
    private:
#endif

        Header      m_Header;
        std::string m_Body;

        struct Info
        {
            MessagePtr message;
            uint32_t   budget  = DEFAULT_WINDOW_SIZE;
            bool       no_body = false;
        };
        std::map<uint32_t, Info> m_Info;
        Inflate                  m_Inflate;

        uint32_t m_Budget = DEFAULT_WINDOW_SIZE; // connection budget

        std::string create_settings(bool aAck)
        {
            Header sHeader;
            sHeader.type = Type::SETTINGS;
            if (aAck)
                sHeader.flags = Flags::ACK_SETTINGS;
            sHeader.to_net();
            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            return sTmp;
        }

        void send_settings(bool aAck)
        {
            m_API->send(create_settings(aAck));
        }

        void send_window_update(uint32_t aStreamId, uint32_t& aCurrent)
        {
            const uint32_t sInc = htobe32(DEFAULT_WINDOW_SIZE); // to network order
            Header         sHeader;
            sHeader.type   = Type::WINDOW_UPDATE;
            sHeader.stream = aStreamId;
            sHeader.size   = sizeof(sInc);
            sHeader.to_net();

            std::string sTmp;
            sTmp.append((const char*)&sHeader, sizeof(sHeader));
            sTmp.append((const char*)&sInc, sizeof(sInc));

            m_API->send(std::move(sTmp));
            aCurrent += DEFAULT_WINDOW_SIZE;
        }

        void recv_hello(std::string& aBuffer)
        {
            if (aBuffer != PREFACE)
                throw Error("http2: invalid preface");
            send_settings(false);
            m_API->established();
            m_Stage = HEADER;
        }

        void recv_header(std::string& aBuffer)
        {
            if (aBuffer.size() != sizeof(Header))
                throw Error("http2: not a frame header");
            Header* sHeader = reinterpret_cast<Header*>(&aBuffer[0]);
            sHeader->to_host();
            m_Header = *sHeader;
            m_Stage  = BODY;
        }

        void recv_body(std::string& aBuffer)
        {
            if (aBuffer.size() != m_Header.size)
                throw Error("http2: not a frame body");
            m_Body = std::move(aBuffer);
            process_packet();
            m_Stage  = HEADER;
            m_Header = {};
            m_Body   = {};
        }

        void push(uint32_t aStreamId, Info& aInfo)
        {
            m_API->process_message(aStreamId, std::move(aInfo.message));
            m_Info.erase(aStreamId);
        }

        // process_xxx can access m_Header + m_Body

        void process_headers()
        {
            const uint32_t sStreamId = m_Header.stream;
            if (sStreamId == 0)
                throw Error("http2: zero stream id");
            auto& sInfo = m_Info[sStreamId];

            if (!sInfo.message)
                sInfo.message = m_API->new_message(sStreamId);
            m_Inflate(m_Header, m_Body, sInfo.message);

            if (m_Header.flags & Flags::END_STREAM)
                sInfo.no_body = true;
            if (m_Header.flags & Flags::END_HEADERS and sInfo.no_body) {
                push(sStreamId, sInfo);
            }
        }

        void process_data()
        {
            const uint32_t sStreamId = m_Header.stream;
            if (sStreamId == 0)
                throw Error("http2: zero stream id");
            auto sIt = m_Info.find(sStreamId);
            if (sIt == m_Info.end())
                throw Error("http2: stream not found");
            auto& sInfo = sIt->second;

            // account budget
            const bool sMore = !(m_Header.flags & Flags::END_STREAM);
            assert(m_Budget >= m_Body.size());
            assert(sInfo.budget >= m_Body.size());
            sInfo.message->append_body(m_Body);
            sInfo.budget -= m_Body.size();
            m_Budget -= m_Body.size();
            if (m_Budget < DEFAULT_WINDOW_SIZE) {
                send_window_update(0, m_Budget);
            }
            if (sInfo.budget < DEFAULT_WINDOW_SIZE and sMore) {
                send_window_update(sStreamId, sInfo.budget);
            }

            // message collected
            if (m_Header.flags & Flags::END_STREAM) {
                push(sStreamId, sInfo);
            }
        }

        void process_settings()
        {
            if (m_Header.flags != 0) // filter out ACK_SETTINGS
                return;
            Container::imemstream sData(m_Body);
            while (!sData.eof()) {
                SettingVal sVal;
                sData.read(sVal);
                sVal.to_host();
                // TRACE(sVal.key << ": " << sVal.value);
            }
            send_settings(true);
        }

        void process_window_update()
        {
            if (m_Body.size() != 4)
                throw Error("http2: invalid window update");
            Container::imemstream sData(m_Body);

            uint32_t sInc = 0;
            sData.read(sInc);
            sInc = be32toh(sInc);
            sInc &= 0x7FFFFFFFFFFFFFFF; // clear R bit

            m_API->window_update(m_Header.stream, sInc);
        }

        void process_packet()
        {
            switch (m_Header.type) {
            case Type::DATA: process_data(); break;
            case Type::HEADERS: process_headers(); break;
            case Type::SETTINGS: process_settings(); break;
            case Type::WINDOW_UPDATE: process_window_update(); break;
            case Type::CONTINUATION: process_headers(); break;
            default: break;
            }
        }

        void client_hello()
        {
            m_API->send(std::string(PREFACE) + create_settings(false));
            m_Stage = CLIENT_WAIT;
        }

        void client_check(std::string& aBuffer)
        {
            recv_header(aBuffer);
            if (m_Header.type != Type::SETTINGS)
                throw Error("http2: not a HTTP/2 peer");
            m_API->established();
        }

    public:
        Main(Mode aMode, API* aAPI)
        : m_Mode(aMode)
        , m_API(aAPI)
        {
            if (m_Mode == CLIENT)
                m_Stage = CLIENT_HELLO;
        }

        size_t hint() const
        {
            switch (m_Stage) {
            case HELLO: return PREFACE.size();
            case HEADER: return sizeof(Header);
            case BODY: return m_Header.size;
            case CLIENT_HELLO: return 0;
            case CLIENT_WAIT: return sizeof(Header);
            }
            throw std::logic_error("http2: invalid parser state");
        }
        void process(std::string& aBuffer)
        {
            // CATAPULT_EVENT("http/2", "parse");
            switch (m_Stage) {
            case HELLO: recv_hello(aBuffer); return;
            case HEADER: recv_header(aBuffer); return;
            case BODY: recv_body(aBuffer); return;
            case CLIENT_HELLO: client_hello(); return;
            case CLIENT_WAIT: client_check(aBuffer); return;
            }
        }
    };

    // link to asio

    struct AsioRequest : IMessage
    {
        Request request;

        virtual void set_header(std::string& aName, std::string& aValue) override
        {
            if (aName == ":method")
                request.method(http::string_to_verb(aValue));
            else if (aName == ":path")
                request.target(aValue);
            else if (aName == ":authority")
                request.set(http::field::host, aValue);
            else if (aName.size() > 1 and aName[0] == ':')
                ;
            else
                request.set(aName, aValue);
        }
        virtual void append_body(std::string& aBody) override
        {
            request.body().append(aBody);
        }
    };

    struct AsioResponse : IMessage
    {
        Response response;

        virtual void set_header(std::string& aName, std::string& aValue) override
        {
            if (aName == ":status")
                response.result(Parser::Atoi<unsigned>(aValue));
            else if (aName.size() > 1 and aName[0] == ':')
                ;
            else
                response.set(aName, aValue);
        }
        virtual void append_body(std::string& aBody) override
        {
            response.body().append(aBody);
        }
    };

} // namespace asio_http::v2::parser