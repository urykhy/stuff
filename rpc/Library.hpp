#pragma once

#include <future>
#include <map>
#include <string>
#include "rpc.pb.h"
#include <event/Error.hpp>

namespace RPC
{
    struct Library
    {
        using Handler = std::function<std::string(const std::string&)>;

    private:
        std::map<std::string, Handler> m_Lib;

    public:
        void insert(const std::string& aName, Handler aHandler)
        {
            m_Lib[aName] = aHandler;
        }

        std::string call(const std::string& aBody) const
        {
            std::string sResult;
            RPC::Response sResponse;
            RPC::Request sRequest;
            if (!sRequest.ParseFromString(aBody))
            {
                sResponse.set_error("protobuf parsing error");
                sResponse.SerializeToString(&sResult);
                return sResult;
            }
            sResponse.set_serial(sRequest.serial());
            const auto sIt = m_Lib.find(sRequest.name());
            if (sIt == m_Lib.end())
            {
                sResponse.set_error("method not found");
                sResponse.SerializeToString(&sResult);
                return sResult;
            }
            try
            {
                sResponse.set_result(sIt->second(sRequest.args()));
            }
            catch(const std::exception& e)
            {
                sResponse.set_error(e.what());
            }
            sResponse.SerializeToString(&sResult);
            return sResult;
        }

        static std::string formatCall(uint64_t aSerial, const std::string& aName, const std::string& aArgs)
        {
            RPC::Request sRequest;
            sRequest.set_serial(aSerial);
            sRequest.set_name(aName);
            sRequest.set_args(aArgs);
            std::string sBuf;
            sRequest.SerializeToString(&sBuf);
            return sBuf;
        }

        static std::pair<uint64_t, std::future<std::string>> parseResponse(const std::string& aStr)
        {
            uint64_t sSerial = 0;
            std::promise<std::string> sPromise;
            RPC::Response sResponse;

            if (!sResponse.ParseFromString(aStr))
            {
                sPromise.set_exception(std::make_exception_ptr(Event::ProtocolError("cant parse protobuf")));
            }
            else
            {
                sSerial = sResponse.serial();
                if (sResponse.has_error())
                    sPromise.set_exception(std::make_exception_ptr(Event::RemoteError(sResponse.error())));
                else
                    sPromise.set_value(sResponse.result());
            }
            return std::make_pair(sSerial, sPromise.get_future());
        }

#ifdef BOOST_CHECK_EQUAL
        RPC::Response debug_call(const std::string& aName, const std::string& aArgs)
        {
            const std::string sCall = formatCall(0, aName, aArgs);
            const std::string sTmp = call(sCall);
            RPC::Response sResponse;
            sResponse.ParseFromString(sTmp);
            return sResponse;
        }
#endif
    };
} // namespace RPC
