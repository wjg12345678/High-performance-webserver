#ifndef FORM_DECODER_H
#define FORM_DECODER_H

#include <map>
#include <string>

class FormDecoder
{
public:
    static std::map<std::string, std::string> parse_urlencoded(const std::string &body);

private:
    static std::string decode_component(const std::string &value);
};

#endif
