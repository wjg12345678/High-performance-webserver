#include "http/form_decoder.h"

#include <cstdlib>
#include <sstream>

std::map<std::string, std::string> FormDecoder::parse_urlencoded(const std::string &body)
{
    std::map<std::string, std::string> result;
    std::stringstream stream(body);
    std::string pair;
    while (std::getline(stream, pair, '&'))
    {
        const size_t pos = pair.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        result[decode_component(pair.substr(0, pos))] =
            decode_component(pair.substr(pos + 1));
    }
    return result;
}

std::string FormDecoder::decode_component(const std::string &value)
{
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '+')
        {
            result.push_back(' ');
        }
        else if (value[i] == '%' && i + 2 < value.size())
        {
            const std::string hex = value.substr(i + 1, 2);
            const char decoded = static_cast<char>(strtol(hex.c_str(), NULL, 16));
            result.push_back(decoded);
            i += 2;
        }
        else
        {
            result.push_back(value[i]);
        }
    }
    return result;
}
