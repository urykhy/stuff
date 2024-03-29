#pragma once

#include <concepts>

#include <boost/beast/http/field.hpp>

#include <archive/LZ4.hpp>
#include <archive/Util.hpp>
#include <asio_http/API.hpp>
#include <format/Json.hpp>
#include <format/List.hpp>
#include <format/Url.hpp>
#include <parser/Atoi.hpp>
#include <parser/Json.hpp>
#include <parser/Url.hpp>

namespace swagger {

    // parse and compress lz4

    inline void pack(const asio_http::Request& aRequest, asio_http::Response& aResponse, std::string&& aData, const size_t aBorder)
    {
        if (aBorder > 0 and aData.size() > aBorder and aRequest[asio_http::http::field::accept_encoding] == "lz4") {
            Archive::WriteLZ4 sFilter;
            aResponse.body() = Archive::filter(aData, &sFilter);
            aResponse.set(asio_http::http::field::content_encoding, "lz4");
        } else {
            aResponse.body() = std::move(aData);
        }
    }

    inline std::string unpack(const asio_http::Response& aResponse)
    {
        if (aResponse[asio_http::http::field::content_encoding] == "lz4") {
            Archive::ReadLZ4 sFilter;
            return Archive::filter(aResponse.body(), &sFilter);
        } else {
            return aResponse.body();
        }
    }

    // format

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::to_json)>
    std::string format(const T& t)
    {
        return Format::Json::to_string(t.to_json());
    }
    inline std::string format(const bool v) { return (v ? "true" : "false"); }
    inline std::string format(const std::string& v) { return v; }
    inline std::string format(const int64_t v) { return std::to_string(v); }
    inline std::string format(double v) { return std::to_string(v); }

    template <class T>
    std::string format(const std::optional<T>& t) { return format(t.value()); }

    template <class T>
    std::string format(const std::vector<T>& v)
    {
        std::stringstream sStream;
        Format::List(
            sStream, v, [](const auto x) {
                return Format::url_encode(format(x));
            },
            ",");
        return sStream.str();
    }

    // parsing

    template <class T>
    requires std::is_member_function_pointer_v<decltype(&T::from_json)>
    void parse(const std::string& s, T& t)
    {
        t.from_json(Parser::Json::parse(s));
    }
    inline void parse(const std::string& s, bool& v) { v = (s == "true" or s == "yes" or s == "1"); }
    inline void parse(const std::string& s, std::string& v) { v = s; }
    inline void parse(const std::string& s, int64_t& v) { v = Parser::Atoi<int64_t>(s); }
    inline void parse(const std::string& s, double& v) { v = Parser::Atof<double>(s); }

    template <class T>
    void parse(const std::string& s, std::optional<T>& v)
    {
        v.emplace();
        parse(s, v.value());
    }

    template <class T>
    void parse(const std::string& s, std::vector<T>& v)
    {
        Parser::simple(
            s,
            [&](auto x) mutable {
                T sTmp;
                parse(Parser::url_decode(x), sTmp);
                v.push_back(std::move(sTmp));
            },
            ',');
    }

    // is_specified

    template <class T>
    bool is_specified(const std::optional<T>& v) { return v.has_value(); }

    template <class T>
    bool is_specified(const std::vector<T>& v) { return !v.empty(); }

    // make header name
    inline boost::beast::string_view header(const char* aName)
    {
        namespace http  = boost::beast::http;
        auto sFieldName = http::string_to_field(aName);
        if (sFieldName != http::field::unknown)
            return http::to_string(sFieldName);
        return boost::beast::string_view(aName);
    }

} // namespace swagger
