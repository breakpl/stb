#pragma once
#include <string>

// Pure C++ JSON format/minify. wx-free so it can be unit-tested without
// initialising the wxWidgets framework.
namespace JsonFormatterUtils {
    // Pretty-prints JSON with 2-space indent. Sets *valid=false for malformed input.
    std::string Format(const std::string& input, bool* valid = nullptr);
    // Strips whitespace outside of string literals.
    std::string Minify(const std::string& input);
}
