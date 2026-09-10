#pragma once
#include <string>

// Pure C++ percent-encode/decode. wx-free so it can be unit-tested without
// initialising the wxWidgets framework.
namespace UrlEncoderUtils {
    // Encodes all bytes except RFC 3986 unreserved chars (A-Z a-z 0-9 - _ . ~).
    // Input is expected to be UTF-8.
    std::string Encode(const std::string& input);
    // Decodes a percent-encoded string; '+' is treated as ' '.
    std::string Decode(const std::string& encoded);
}
