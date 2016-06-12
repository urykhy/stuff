
#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <list>
#include <map>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

namespace CSV {

    struct csv_separator;
    typedef boost::tokenizer<csv_separator> Tokenizer;

    struct csv_error : public std::runtime_error {
        csv_error(const std::string& what_arg):std::runtime_error(what_arg) { }
    };

    class csv_separator
    {
        bool hit_last = false;
        bool is_quote(char e) {  return e == '\''; }
        bool is_escape(char e) { return e == '\\'; }
        bool is_separator(char e) { return e == ','; }
    public:

        template <typename InputIterator, typename Token>
        bool operator()(InputIterator& next,InputIterator end,Token& tok)
        {
            bool bInQuote = false;
            tok = Token();

            if (next == end) {
                bool rc = !hit_last;
                hit_last = true;
                return rc;
            }
            if (is_quote(*next)) {
                bInQuote = true;
                next++;
            }
            for (;next != end;++next) {
                if (bInQuote) {
                    if (is_escape(*next)) {
                        if (next+1 != end && is_quote(*(next+1))) {
                            next++;
                            tok += *next;
                            continue;
                        }
                    }
                    if (is_quote(*next)) {
                        bInQuote = false;
                        continue;
                    }
                } else {
                    if (is_separator(*next)) {
                        next++;
                        return true;
                    }
                    if (is_escape(*next)) {
                        if (next+1 != end) {
                            next++;
                            tok += *next;
                            continue;
                        } else {
                            throw csv_error("unexpected escape symbol");
                        }
                    }
                }
                tok += *next;
            }
            hit_last = true;
            return true;
        }
        void reset() {
            hit_last = false;
        }
    };

    template<class T>
    size_t read(const std::string& fname, T t) {
        std::ifstream in(fname);
        size_t counter = 0;
        std::string line;
        while (std::getline(in,line)) {
            counter++;
            try {
                boost::trim(line);  // trim CR/LF characters
                t(line);
            } catch (std::exception& e) {
                std::cerr << "fail to parse for: " << line << " : " << e.what() << std::endl;
            }
        }
        return counter;
    }

    template<std::size_t I = 0, typename F, typename... Tp>
    inline typename std::enable_if<I == sizeof...(Tp), void>::type
    tuple_for_each(F f, std::tuple<Tp...>& t)
    { }

    template<std::size_t I = 0, typename F, typename... Tp>
    inline typename std::enable_if<I < sizeof...(Tp), void>::type
    tuple_for_each(F f, std::tuple<Tp...>& t)
    {
        f(std::get<I>(t));
        tuple_for_each<I + 1, F, Tp...>(f, t);
    }

    class ItemWalker {
        Tokenizer::iterator iter;
        Tokenizer::iterator end;
    public:
        ItemWalker(Tokenizer::iterator i, Tokenizer::iterator e) : iter(i), end(e) {}
        void operator()(size_t& i) {
            if (iter != end) {
                i = std::stoul(*iter++);
            }
        }
        void operator()(double& i) {
            if (iter != end) {
                i = std::stod(*iter++);
            }
        }
        template<class T, class V>
        void operator()(std::pair<T, V>& i) {
            operator()(i.first);
            operator()(i.second);
        }
        void operator()(std::string& i) {
            if (iter != end) {
                i = *iter++;
            }
        }
    };

    template<class T>
    void process_line(const std::string& line, T& dest)
    {
        csv_separator sep;
        Tokenizer tok(line, sep);
        Tokenizer::iterator s = tok.begin();

        ItemWalker ir(s, tok.end());
        tuple_for_each(ir, dest);
    }

    template<class T>
    void process_file(const std::string& fname, std::list<T>& dest)
    {
        read(fname, [&dest](const std::string& line){
            T entry;
            process_line(line, entry);
            dest.push_back(entry);
        });
    }
}

