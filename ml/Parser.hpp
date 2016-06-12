
#pragma once
#include <Csv.hpp>

template<typename T>
struct Parser
{
    typename T::List operator()(const std::string& fname, const std::string& s_true)
    {
        typename T::List res;

        bool header = true;
        CSV::read(fname, [&res, &s_true, &header](const std::string& line) {
            if (header) {
                header = false;
                return;
            }
            typename T::Entry entry;
            CSV::csv_separator sep;
            CSV::Tokenizer tok(line, sep);
            CSV::Tokenizer::iterator s = tok.begin();

            if (T::initial_one) {
                entry.second[0] = 1.; // LR requirement
            }
            for (size_t i = T::initial_one; i < T::max_features; i++) {
                entry.second[i] = std::stod(*s++);
            }
            if (*s == s_true) entry.first = true;
            else              entry.first = false;

            res.push_back(entry);
        });

        // normalize
        for (size_t i = T::initial_one; i < T::max_features; i++) {
            double m = 1.0;
            for (auto& n : res) {
                m = std::max(m, n.second[i]);
            }
            if (m > 1) {
                for (auto& n : res) {
                    n.second[i] = n.second[i] / m;
                }
            }
        }
        return res;
    }
};

