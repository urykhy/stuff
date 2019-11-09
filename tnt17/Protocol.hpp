
#include <serialize/MsgPack.hpp>

namespace tnt17
{
    using binary = MsgPack::binary;
    using omemstream = MsgPack::omemstream;
    using imemstream = MsgPack::imemstream;

    inline void assertProto(bool aFlag)
    {
        if (!aFlag)
            throw Event::ProtocolError("bad msgpack message");
    }

    struct Header
    {
        uint64_t code = 0;
        uint64_t sync = 0;
        uint64_t schema_id = 0;

        void parse(imemstream& aStream)
        {
            using namespace MsgPack;
            int tmp = 0;
            tmp = read_map_size(aStream); assertProto(tmp == 3);
            read_uint(aStream, tmp); assertProto(tmp == 0); read_uint(aStream, code);
            read_uint(aStream, tmp); assertProto(tmp == 1); read_uint(aStream, sync);
            read_uint(aStream, tmp); assertProto(tmp == 5); read_uint(aStream, schema_id);
        }
    };

    struct Reply
    {
        bool ok = false;
        std::string error;

        void parse(imemstream& aStream)
        {
            using namespace MsgPack;
            int tmp = read_map_size(aStream);
            if (0 == tmp)
            {
                ok = true;
                return;
            }
            assertProto(tmp == 1);
            read_uint(aStream, tmp);
            if (tmp == 0x31)
                read_string(aStream, error);
            else if (tmp == 0x30)
                ok = true;
            else
                assertProto(false);
        }
    };

    enum
    {
        CODE_SELECT = 1,
        CODE_INSERT = 2,
        CODE_DELETE = 5,
        CODE_AUTH   = 7,
        CODE_CALL   = 0xa
    };

    template<class P>
    void formatHeader(P& aStream, int aCode, int aSync)
    {
        using namespace MsgPack;
        write_map_size(aStream, 2);
        write_uint(aStream, 0);    write_uint(aStream, aCode);
        write_uint(aStream, 1);    write_uint(aStream, aSync);
    }

    template<class S>
    void formatSelectBody(S& aStream, int aSpaceId, int aIndexId)
    {
        using namespace MsgPack;
        write_map_size(aStream, 6);
        write_uint(aStream, 0x10);    write_uint(aStream, aSpaceId);
        write_uint(aStream, 0x11);    write_uint(aStream, aIndexId);
        write_uint(aStream, 0x12);    write_uint(aStream, 255); // limit
        write_uint(aStream, 0x13);    write_uint(aStream, 0);   // offset
        write_uint(aStream, 0x14);    write_uint(aStream, 0);   // iterator
        write_uint(aStream, 0x20);    write_array_size(aStream, 1);
    }
#if 0
    template<class P, class V>
    void formatInsertBody(P& aStream, int aSpaceId, const V& aValue)
    {
        using namespace MsgPack;
        write_map_size(aStream, 2);
        write(aStream, 0x10);    write(aStream, aSpaceId);
        write(aStream, 0x21);    aValue.write(aStream);
    }

    template<class P>
    void formatDeleteBody(P& aStream, int aSpaceId, int aIndexId, uint64_t aKey)
    {
        using namespace MsgPack;
        write_map_size(aStream, 3);
        write(aStream, 0x10);    write(aStream, aSpaceId);
        write(aStream, 0x11);    write(aStream, aIndexId);
        write(aStream, 0x20);    write_array_size(aStream, 1);    write(aStream, aKey);
    }

    template<class P, class A>
    void formatCallBody(P& aStream, const std::string& aName, const A& aArgs)
    {
        using namespace MsgPack;
        write_map_size(aStream, 2);
        write(aStream, 0x22);    write(aStream, aName);
        write(aStream, 0x21);    write_array_size(aStream, 1);    write(aStream, aArgs);
    }

    template<class P>
    void formatAuthBody(P& aStream, const std::string& aName, const boost::string_ref& aHash)
    {
        using namespace MsgPack;
        write_map_size(aStream, 2);
        write(aStream, 0x23);    write(aStream, aName);
        write(aStream, 0x21);    write_array_size(aStream, 2);    write(aStream, "chap-sha1");     write(aStream, aHash);
    }

    template<class U, class T>
    void parseReply(const T& aData, Header& aHeader, std::vector<U>& aResult)
    {
        using namespace MsgPack;
        imemstream sStream(aData);
        aHeader.parse(sStream);

        Reply sReply;
        sReply.parse(sStream);

        if (sReply.ok)
        {
            uint32_t sReplyCount = read_array_size(sStream);
            aResult.resize(sReplyCount);
            for (auto& x : aResult)
                x.parse(sStream);
        } else {
            throw std::runtime_error(std::string(sReply.error.data(), sReply.error.size()));
        }
    }
#endif
}