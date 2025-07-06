#pragma once

#include <exception>

#include <boost/noncopyable.hpp>

#include <avro/Decoder.hh>
#include <avro/Encoder.hh>
#include <avro/Exception.hh>
#include <avro/Generic.hh>
#include <avro/Specific.hh>
#include <libserdes/serdescpp-avro.h>
#include <unsorted/Env.hpp>

namespace Kafka {

    // based on https://github.com/confluentinc/libserdes/blob/master/examples/kafka-serdes-avro-console-producer.cpp
    class Registry : public boost::noncopyable
    {
        std::unique_ptr<Serdes::Conf> m_Config;
        std::unique_ptr<Serdes::Avro> m_Serdes;

        static std::unique_ptr<avro::GenericDatum> ToAvro(std::string_view aJson, Serdes::Schema* aSchema)
        {
            auto                                sSchema  = aSchema->object();
            auto                                sInput   = avro::memoryInputStream((const uint8_t*)aJson.data(), aJson.size());
            avro::DecoderPtr                    sDecoder = avro::jsonDecoder(*sSchema);
            std::unique_ptr<avro::GenericDatum> sTmp(new avro::GenericDatum(*sSchema));
            sDecoder->init(*sInput);
            avro::decode(*sDecoder, *sTmp);
            return sTmp;
        }

        static std::string FromAvro(avro::GenericDatum* aTmp, Serdes::Schema* aSchema)
        {
            auto               sSchema  = aSchema->object();
            avro::EncoderPtr   sEncoder = avro::jsonEncoder(*sSchema);
            std::ostringstream sStream;
            auto               sOutput = avro::ostreamOutputStream(sStream);
            sEncoder->init(*sOutput.get());
            avro::encode(*sEncoder, *aTmp);
            sEncoder->flush();
            return sStream.str();
        }

    public:
        Registry()
        {
            m_Config.reset(Serdes::Conf::create());

            std::string sErr;
            if (m_Config->set("schema.registry.url", Util::getEnv("KAFKA_REGISTRY_URL"), sErr)) {
                throw std::invalid_argument("Registry: " + sErr);
            }
            if (m_Config->set("serializer.framing", "cp1", sErr)) {
                throw std::invalid_argument("Registry: " + sErr);
            }

            m_Serdes.reset(Serdes::Avro::create(m_Config.get(), sErr));
            if (!m_Serdes) {
                throw std::runtime_error("Registry: fail to create handle: " + sErr);
            }
        }

        std::unique_ptr<Serdes::Schema> GetSchema(const std::string& aName)
        {
            std::string sErr;
            auto        sSchema = Serdes::Schema::get(m_Serdes.get(), aName, sErr);
            INFO("Request schema " << aName);
            if (!sSchema) {
                throw std::runtime_error("Registry: fail to get schema " + aName + ": " + sErr);
            }
            INFO("Got schema " << aName);
            return std::unique_ptr<Serdes::Schema>(sSchema);
        }

        std::unique_ptr<Serdes::Schema> CreateSchema(const std::string& aName, const std::string& aDef)
        {
            std::string sErr;
            auto        sSchema = Serdes::Schema::add(m_Serdes.get(), aName, aDef, sErr);
            if (!sSchema) {
                throw std::runtime_error("Registry: fail to add schema " + aName + ": " + sErr);
            }
            INFO("Created schema " << aName);
            return std::unique_ptr<Serdes::Schema>(sSchema);
        }

        std::unique_ptr<Serdes::Schema> GetOrCreate(const std::string& aName, const std::string& aDef)
        {
            std::unique_ptr<Serdes::Schema> sCurrent;
            try {
                return GetSchema(aName);
            } catch (...) {
                return CreateSchema(aName, aDef);
            }
        }

        std::vector<char> Encode(std::string_view aJson, Serdes::Schema* aSchema)
        {
            auto              sTmp = ToAvro(aJson, aSchema);
            std::vector<char> sOut;
            std::string       sErr;
            if (m_Serdes->serialize(aSchema, sTmp.get(), sOut, sErr) == -1) {
                throw std::runtime_error("Serdes: fail to encode: " + sErr);
            }
            return sOut;
        }

        std::string Decode(std::string_view aMessage)
        {
            avro::GenericDatum* sTmp    = nullptr;
            Serdes::Schema*     sSchema = nullptr;
            std::string         sErr;
            if (m_Serdes->deserialize(&sSchema, &sTmp, aMessage.data(), aMessage.size(), sErr) == -1) {
                throw std::runtime_error("Serdes: fail to decode: " + sErr);
            }
            std::unique_ptr<avro::GenericDatum> sTmpPtr(sTmp);
            return FromAvro(sTmp, sSchema);
        }
    };

} // namespace Kafka
