#include "Base64Utils.h"
#include <array>
#include <cctype>

static const char kEncodeTable[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::array<int, 256> BuildDecodeTable() {
    std::array<int, 256> t;
    t.fill(-1);
    for (int i = 0; i < 26; i++) { t['A' + i] = i; t['a' + i] = i + 26; }
    for (int i = 0; i < 10; i++) { t['0' + i] = i + 52; }
    t[static_cast<unsigned char>('+')] = 62;
    t[static_cast<unsigned char>('/')] = 63;
    return t;
}

static const std::array<int, 256> kDecodeTable = BuildDecodeTable();

std::string Base64Utils::Encode(const std::string& input) {
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    const auto* p = reinterpret_cast<const unsigned char*>(input.data());
    size_t rem = input.size();

    while (rem >= 3) {
        out += kEncodeTable[p[0] >> 2];
        out += kEncodeTable[((p[0] & 3) << 4) | (p[1] >> 4)];
        out += kEncodeTable[((p[1] & 0xf) << 2) | (p[2] >> 6)];
        out += kEncodeTable[p[2] & 0x3f];
        p += 3; rem -= 3;
    }
    if (rem == 1) {
        out += kEncodeTable[p[0] >> 2];
        out += kEncodeTable[(p[0] & 3) << 4];
        out += '='; out += '=';
    } else if (rem == 2) {
        out += kEncodeTable[p[0] >> 2];
        out += kEncodeTable[((p[0] & 3) << 4) | (p[1] >> 4)];
        out += kEncodeTable[(p[1] & 0xf) << 2];
        out += '=';
    }
    return out;
}

std::string Base64Utils::Decode(const std::string& input, bool* valid) {
    std::string in;
    in.reserve(input.size());
    for (unsigned char c : input) {
        if (!std::isspace(c)) in += static_cast<char>(c);
    }

    if (in.empty()) {
        if (valid) *valid = true;
        return "";
    }

    if (in.size() % 4 != 0) {
        if (valid) *valid = false;
        return "";
    }

    std::string out;
    out.reserve((in.size() / 4) * 3);

    for (size_t i = 0; i < in.size(); i += 4) {
        int v0 = kDecodeTable[static_cast<unsigned char>(in[i])];
        int v1 = kDecodeTable[static_cast<unsigned char>(in[i + 1])];

        if (v0 < 0 || v1 < 0) { if (valid) *valid = false; return ""; }

        bool pad2 = (in[i + 2] == '=');
        bool pad3 = (in[i + 3] == '=');

        if (pad2 && !pad3) { if (valid) *valid = false; return ""; }

        int v2 = pad2 ? 0 : kDecodeTable[static_cast<unsigned char>(in[i + 2])];
        int v3 = pad3 ? 0 : kDecodeTable[static_cast<unsigned char>(in[i + 3])];

        if (!pad2 && v2 < 0) { if (valid) *valid = false; return ""; }
        if (!pad3 && v3 < 0) { if (valid) *valid = false; return ""; }

        if ((pad2 || pad3) && i + 4 != in.size()) { if (valid) *valid = false; return ""; }

        out += static_cast<char>((v0 << 2) | (v1 >> 4));
        if (!pad2) out += static_cast<char>(((v1 & 0xf) << 4) | (v2 >> 2));
        if (!pad3) out += static_cast<char>(((v2 & 3) << 6) | v3);
    }

    if (valid) *valid = true;
    return out;
}
