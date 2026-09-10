#include "JsonFormatterUtils.h"

std::string JsonFormatterUtils::Format(const std::string& input, bool* valid) {
    std::string result;
    int indent = 0;
    bool inString = false;
    bool escape = false;
    bool hasError = false;

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];

        if (escape) { result += c; escape = false; continue; }
        if (c == '\\' && inString) { result += c; escape = true; continue; }
        if (c == '"') { inString = !inString; result += c; continue; }
        if (inString) { result += c; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;

        switch (c) {
            case '{': case '[':
                result += c; result += '\n';
                indent++;
                result += std::string(static_cast<size_t>(indent) * 2, ' ');
                break;
            case '}': case ']':
                result += '\n';
                if (indent > 0) indent--;
                else hasError = true;
                result += std::string(static_cast<size_t>(indent) * 2, ' ');
                result += c;
                break;
            case ',':
                result += c; result += '\n';
                result += std::string(static_cast<size_t>(indent) * 2, ' ');
                break;
            case ':':
                result += ": ";
                break;
            default:
                result += c;
        }
    }

    if (valid) *valid = !inString && indent == 0 && !hasError;
    return result;
}

std::string JsonFormatterUtils::Minify(const std::string& input) {
    std::string result;
    bool inString = false;
    bool escape = false;

    for (size_t i = 0; i < input.size(); i++) {
        char c = input[i];

        if (escape) { result += c; escape = false; continue; }
        if (c == '\\' && inString) { result += c; escape = true; continue; }
        if (c == '"') { inString = !inString; result += c; continue; }
        if (inString) { result += c; continue; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        result += c;
    }

    return result;
}
