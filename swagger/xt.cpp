#include <regex>

template class std::basic_regex<char>;

template bool std::regex_match(
    const std::string&,
    const std::regex&,
    std::regex_constants::match_flag_type);
