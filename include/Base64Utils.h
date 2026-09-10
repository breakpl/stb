#pragma once
#include <string>

// Pure C++ base64 encode/decode. wx-free so it can be unit-tested without
// initialising the wxWidgets framework.
namespace Base64Utils {
    std::string Encode(const std::string& input);
    // Decodes base64, skipping any embedded whitespace.
    // Sets *valid=false and returns "" on invalid input.
    std::string Decode(const std::string& input, bool* valid = nullptr);
}
