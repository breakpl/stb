#include "UrlEncoderUtils.h"
#include <cstdio>
#include <cstdlib>

static bool IsUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

std::string UrlEncoderUtils::Encode(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 3);
    for (unsigned char c : input) {
        if (IsUnreserved(c)) {
            result += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            result += buf;
        }
    }
    return result;
}

std::string UrlEncoderUtils::Decode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.size());
    size_t len = encoded.size();
    for (size_t i = 0; i < len; ++i) {
        char c = encoded[i];
        if (c == '%' && i + 2 < len) {
            char hex[3] = { encoded[i + 1], encoded[i + 2], '\0' };
            char* end = nullptr;
            unsigned long val = std::strtoul(hex, &end, 16);
            if (end == hex + 2) {
                result += static_cast<char>(val);
                i += 2;
                continue;
            }
        } else if (c == '+') {
            result += ' ';
            continue;
        }
        result += c;
    }
    return result;
}
