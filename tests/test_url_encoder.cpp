#include <catch2/catch_test_macros.hpp>
#include "UrlEncoderUtils.h"

// ── Encode ────────────────────────────────────────────────────────────────────

TEST_CASE("Encode empty string returns empty", "[url]") {
    REQUIRE(UrlEncoderUtils::Encode("") == "");
}

TEST_CASE("Unreserved chars pass through unchanged", "[url]") {
    std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    REQUIRE(UrlEncoderUtils::Encode(unreserved) == unreserved);
}

TEST_CASE("Space is encoded as %20", "[url]") {
    REQUIRE(UrlEncoderUtils::Encode("hello world") == "hello%20world");
}

TEST_CASE("Special URL-significant chars are encoded", "[url]") {
    REQUIRE(UrlEncoderUtils::Encode("foo=bar&baz=qux") == "foo%3Dbar%26baz%3Dqux");
}

TEST_CASE("Encode uses uppercase hex digits", "[url]") {
    REQUIRE(UrlEncoderUtils::Encode("\x0f") == "%0F");
    REQUIRE(UrlEncoderUtils::Encode("\xff") == "%FF");
}

// ── Decode ────────────────────────────────────────────────────────────────────

TEST_CASE("Decode empty string returns empty", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("") == "");
}

TEST_CASE("Decode percent-encoded space", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("hello%20world") == "hello world");
}

TEST_CASE("Decode '+' as space", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("hello+world") == "hello world");
}

TEST_CASE("Decode mixed percent-encoded and plain chars", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("foo%3Dbar%26baz%3Dqux") == "foo=bar&baz=qux");
}

TEST_CASE("Decode accepts lowercase hex digits", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("hello%20world") == "hello world");
    REQUIRE(UrlEncoderUtils::Decode("%2f") == "/");
}

TEST_CASE("Lone percent at end of string is preserved literally", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("foo%") == "foo%");
}

TEST_CASE("Percent with one hex digit at end is preserved literally", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("foo%2") == "foo%2");
}

TEST_CASE("Percent followed by invalid hex chars is preserved literally", "[url]") {
    REQUIRE(UrlEncoderUtils::Decode("foo%GG") == "foo%GG");
}

// ── Roundtrip ─────────────────────────────────────────────────────────────────

TEST_CASE("Decode(Encode(s)) == s for a query string", "[url]") {
    std::string original = "Hello, World! foo=bar&baz=qux";
    REQUIRE(UrlEncoderUtils::Decode(UrlEncoderUtils::Encode(original)) == original);
}

TEST_CASE("Decode(Encode(s)) == s for all ASCII printable chars", "[url]") {
    std::string printable;
    for (int c = 32; c < 127; c++) printable += static_cast<char>(c);
    REQUIRE(UrlEncoderUtils::Decode(UrlEncoderUtils::Encode(printable)) == printable);
}
