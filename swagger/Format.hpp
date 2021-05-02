#pragma once

#include <format/Json.hpp>
#include <format/List.hpp>
#include <format/Url.hpp>
#include <parser/Atoi.hpp>
#include <parser/Json.hpp>
#include <parser/Url.hpp>

namespace swagger {

    // format

    std::string format(const bool v) { return (v ? "true" : "false"); }
    std::string format(const std::string& v) { return v; }
    std::string format(const uint64_t v) { return std::to_string(v); }
    std::string format(double v) { return std::to_string(v); }

    template <class T>
    std::string format(const std::vector<T>& v)
    {
        std::stringstream sStream;
        Format::List(
            sStream, v, [](const auto x) {
                return format(x);
            },
            ",");
        return sStream.str();
    }

    template <class T>
    typename std::enable_if<Format::Json::has_to_json_v<T>, std::string>::type format(const T& t)
    {
        return Format::Json::to_string(t.to_json());
    }

    template <class T>
    std::string format(const std::optional<T>& t) { return format(t.value()); }

    // parsing

    void parse(const std::string& s, bool& v) { v = (s == "true" or s == "yes" or s == "1"); }
    void parse(const std::string& s, std::string& v) { v = s; }
    void parse(const std::string& s, uint64_t& v) { v = Parser::Atoi<uint64_t>(s); }
    void parse(const std::string& s, double& v) { v = Parser::Atof<double>(s); }

    template <class T>
    void parse(const std::string& s, std::vector<T>& v)
    {
        Parser::simple(
            s,
            [&](auto x) mutable {
                T sTmp;
                parse(std::string(x), sTmp);
                v.push_back(std::move(sTmp));
            },
            ',');
    }

    template <class T>
    typename std::enable_if<Parser::Json::has_from_json_v<T>, void>::type parse(const std::string& s, T& v)
    {
        v.from_json(Parser::Json::parse(s));
    }

    template <class T>
    void parse(const std::string& s, std::optional<T>& v)
    {
        v.emplace();
        parse(s, v.value());
    }

    // is_specified

    template <class T>
    bool is_specified(const std::optional<T>& v) { return v.has_value(); }

    template<class T>
    bool is_specified(const std::vector<T>& v) { return !v.empty(); }

} // namespace swagger
