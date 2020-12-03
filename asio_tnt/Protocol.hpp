
#include <mpl/Mpl.hpp>
#include <msgpack/MsgPack.hpp>

#include "Error.hpp"

namespace asio_tnt {
    using binary     = MsgPack::binary;
    using omemstream = MsgPack::omemstream;
    using imemstream = MsgPack::imemstream;

    inline void assertProto(bool aFlag)
    {
        if (!aFlag)
            throw NetworkError(boost::system::errc::protocol_error);
    }

    struct Header
    {
        uint64_t code      = 0;
        uint64_t sync      = 0;
        uint64_t schema_id = 0;

        void parse(imemstream& aStream)
        {
            using namespace MsgPack;
            int tmp = 0;
            tmp     = read_map_size(aStream);
            assertProto(tmp == 3);
            read_uint(aStream, tmp);
            assertProto(tmp == 0);
            read_uint(aStream, code);
            read_uint(aStream, tmp);
            assertProto(tmp == 1);
            read_uint(aStream, sync);
            read_uint(aStream, tmp);
            assertProto(tmp == 5);
            read_uint(aStream, schema_id);
        }
    };

    struct Reply
    {
        bool        ok = false;
        std::string error;

        void parse(imemstream& aStream)
        {
            using namespace MsgPack;
            int tmp = read_map_size(aStream);
            if (0 == tmp) {
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

    struct IndexSpec
    {
        unsigned id       = 0;
        unsigned limit    = 255;
        unsigned offset   = 0;
        unsigned iterator = 0;

        IndexSpec& set_id(unsigned x)
        {
            id = x;
            return *this;
        }
        IndexSpec& set_limit(unsigned x)
        {
            limit = x;
            return *this;
        }
        IndexSpec& set_offset(unsigned x)
        {
            offset = x;
            return *this;
        }
        IndexSpec& set_iterator(unsigned x)
        {
            iterator = x;
            return *this;
        }

        enum
        {
            ITER_EQ  = 0, // key == x ASC order
            ITER_REQ = 1, // key == x DESC order
            ITER_ALL = 2, // all tuples
            ITER_LT  = 3, // key <  x
            ITER_LE  = 4, // key <= x
            ITER_GE  = 5, // key >= x
            ITER_GT  = 6, // key >  x
        };
    };

    class Protocol
    {
        std::atomic<uint64_t> m_Serial{1};
        const unsigned        m_Space; // tnt space id

        enum
        {
            CODE_SELECT = 1,
            CODE_INSERT = 2,
            CODE_DELETE = 5,
            CODE_AUTH   = 7,
            CODE_CALL   = 0xa
        };

        static void write(MsgPack::omemstream& aStream, const std::string& aKey)
        {
            MsgPack::write_string(aStream, aKey);
        }
        static void write(omemstream& aStream, const uint64_t& aKey)
        {
            MsgPack::write_uint(aStream, aKey);
        }

        void formatHeader(MsgPack::omemstream& aStream, int aCode, int aSync)
        {
            using namespace MsgPack;
            write_map_size(aStream, 2);
            write_uint(aStream, 0);
            write_uint(aStream, aCode);
            write_uint(aStream, 1);
            write_uint(aStream, aSync);
        }

        void formatSelectBody(MsgPack::omemstream& aStream, const IndexSpec& aIndex)
        {
            using namespace MsgPack;
            write_map_size(aStream, 6);
            write_uint(aStream, 0x10);
            write_uint(aStream, m_Space);
            write_uint(aStream, 0x11);
            write_uint(aStream, aIndex.id);
            write_uint(aStream, 0x12);
            write_uint(aStream, aIndex.limit);
            write_uint(aStream, 0x13);
            write_uint(aStream, aIndex.offset);
            write_uint(aStream, 0x14);
            write_uint(aStream, aIndex.iterator);
            write_uint(aStream, 0x20);
            write_array_size(aStream, 1);
        }

        template <class V>
        void formatInsertBody(MsgPack::omemstream& aStream, const V& aValue)
        {
            using namespace MsgPack;
            write_map_size(aStream, 2);
            write_uint(aStream, 0x10);
            write_uint(aStream, m_Space);
            write_uint(aStream, 0x21);
            aValue.serialize(aStream);
        }

        void formatDeleteBody(MsgPack::omemstream& aStream, const IndexSpec& aIndex)
        {
            using namespace MsgPack;
            write_map_size(aStream, 3);
            write_uint(aStream, 0x10);
            write_uint(aStream, m_Space);
            write_uint(aStream, 0x11);
            write_uint(aStream, aIndex.id);
            write_uint(aStream, 0x20);
            write_array_size(aStream, 1);
        }

        template <class... A>
        void formatCallBody(MsgPack::omemstream& aStream, const std::string& aName, const A&... aArgs)
        {
            using namespace MsgPack;
            write_map_size(aStream, 2);
            write_uint(aStream, 0x22);
            write_string(aStream, aName);
            write_uint(aStream, 0x21);
            write_array_size(aStream, sizeof...(A));
            Mpl::for_each_argument(
                [&aStream](const auto& x) {
                    write(aStream, x);
                },
                aArgs...);
        }

    public:
        Protocol(unsigned aSpace)
        : m_Space(aSpace)
        {}

        struct Request
        {
            uint64_t    serial = 0;
            std::string body;
        };

        template <class K>
        Request formatSelect(const IndexSpec& aIndex, const K& aKey)
        {
            const uint64_t      sSerial = m_Serial++;
            MsgPack::omemstream sStream;
            formatHeader(sStream, CODE_SELECT, sSerial);
            formatSelectBody(sStream, aIndex);
            write(sStream, aKey);
            return Request{sSerial, sStream.str()};
        }

        template <class T>
        Request formatInsert(const T& aData)
        {
            const uint64_t      sSerial = m_Serial++;
            MsgPack::omemstream sStream;
            formatHeader(sStream, CODE_INSERT, sSerial);
            formatInsertBody(sStream, aData);
            return Request{sSerial, sStream.str()};
        }

        template <class K>
        Request formatDelete(const IndexSpec& aIndex, const K& aKey)
        {
            const uint64_t      sSerial = m_Serial++;
            MsgPack::omemstream sStream;
            formatHeader(sStream, CODE_DELETE, sSerial);
            formatDeleteBody(sStream, aIndex);
            write(sStream, aKey);
            return Request{sSerial, sStream.str()};
        }

        template <class... A>
        Request formatCall(const std::string& aName, const A&... aArgs)
        {
            const uint64_t      sSerial = m_Serial++;
            MsgPack::omemstream sStream;
            formatHeader(sStream, CODE_CALL, sSerial);
            formatCallBody(sStream, aName, aArgs...);
            return Request{sSerial, sStream.str()};
        }
    };

#if 0
    template<class P>
    void formatAuthBody(P& aStream, const std::string& aName, const boost::string_ref& aHash)
    {
        using namespace MsgPack;
        write_map_size(aStream, 2);
        write(aStream, 0x23);    write(aStream, aName);
        write(aStream, 0x21);    write_array_size(aStream, 2);    write(aStream, "chap-sha1");     write(aStream, aHash);
    }
#endif
} // namespace asio_tnt
