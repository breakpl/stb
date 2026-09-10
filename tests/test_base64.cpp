#include <catch2/catch_test_macros.hpp>
#include "Base64Utils.h"

// ── Encode ────────────────────────────────────────────────────────────────────

TEST_CASE("Encode empty string returns empty", "[base64]") {
    REQUIRE(Base64Utils::Encode("") == "");
}

TEST_CASE("Encode 3-byte input produces 4 chars with no padding", "[base64]") {
    REQUIRE(Base64Utils::Encode("Man") == "TWFu");
}

TEST_CASE("Encode 2-byte input produces 4 chars with one padding char", "[base64]") {
    REQUIRE(Base64Utils::Encode("Ma") == "TWE=");
}

TEST_CASE("Encode 1-byte input produces 4 chars with two padding chars", "[base64]") {
    REQUIRE(Base64Utils::Encode("M") == "TQ==");
}

TEST_CASE("Encode known string matches expected base64", "[base64]") {
    REQUIRE(Base64Utils::Encode("Hello, World!") == "SGVsbG8sIFdvcmxkIQ==");
}

// ── Decode ────────────────────────────────────────────────────────────────────

TEST_CASE("Decode empty string is valid and returns empty", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("", &valid) == "");
    REQUIRE(valid);
}

TEST_CASE("Decode whitespace-only input is valid and returns empty", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("  \t\n  ", &valid) == "");
    REQUIRE(valid);
}

TEST_CASE("Decode known base64 string", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("TWFu", &valid) == "Man");
    REQUIRE(valid);
}

TEST_CASE("Decode skips embedded whitespace", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("TW Fu", &valid) == "Man");
    REQUIRE(valid);
}

TEST_CASE("Decode with single padding char", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("TWE=", &valid) == "Ma");
    REQUIRE(valid);
}

TEST_CASE("Decode with double padding chars", "[base64]") {
    bool valid = false;
    REQUIRE(Base64Utils::Decode("TQ==", &valid) == "M");
    REQUIRE(valid);
}

TEST_CASE("Decode invalid base64 characters sets valid=false", "[base64]") {
    bool valid = true;
    Base64Utils::Decode("not!valid@base#64", &valid);
    REQUIRE_FALSE(valid);
}

TEST_CASE("Decode wrong length (missing padding) sets valid=false", "[base64]") {
    bool valid = true;
    Base64Utils::Decode("TWF", &valid);
    REQUIRE_FALSE(valid);
}

TEST_CASE("Decode with padding in non-final group sets valid=false", "[base64]") {
    bool valid = true;
    Base64Utils::Decode("TQ==TWFu", &valid);
    REQUIRE_FALSE(valid);
}

// ── Roundtrip ─────────────────────────────────────────────────────────────────

TEST_CASE("Decode(Encode(s)) == s for plain ASCII", "[base64]") {
    bool valid = false;
    std::string s = "Hello, World! This is a test.";
    REQUIRE(Base64Utils::Decode(Base64Utils::Encode(s), &valid) == s);
    REQUIRE(valid);
}

TEST_CASE("Decode(Encode(s)) == s for all 256 byte values", "[base64]") {
    std::string binary;
    binary.reserve(256);
    for (int i = 0; i < 256; i++) binary += static_cast<char>(i);
    bool valid = false;
    REQUIRE(Base64Utils::Decode(Base64Utils::Encode(binary), &valid) == binary);
    REQUIRE(valid);
}
