#include <catch2/catch_test_macros.hpp>
#include "TimestampUtils.h"

// 2024-02-25 12:34:56 UTC
static const time_t       REF_TS   = 1708864496;
static const std::string  REF_ZULU = "2024-02-25T12:34:56Z";

// ── ToZulu ────────────────────────────────────────────────────────────────────

TEST_CASE("ToZulu formats Unix epoch as 1970-01-01T00:00:00Z", "[timestamp]") {
    REQUIRE(TimestampUtils::ToZulu(0) == "1970-01-01T00:00:00Z");
}

TEST_CASE("ToZulu formats a known timestamp correctly", "[timestamp]") {
    REQUIRE(TimestampUtils::ToZulu(REF_TS) == REF_ZULU);
}

TEST_CASE("ToZulu output has correct YYYY-MM-DDTHH:MM:SSZ shape", "[timestamp]") {
    std::string s = TimestampUtils::ToZulu(REF_TS);
    REQUIRE(s.size() == 20);
    REQUIRE(s[4]  == '-');
    REQUIRE(s[7]  == '-');
    REQUIRE(s[10] == 'T');
    REQUIRE(s[13] == ':');
    REQUIRE(s[16] == ':');
    REQUIRE(s[19] == 'Z');
}

// ── FromZulu ──────────────────────────────────────────────────────────────────

TEST_CASE("FromZulu parses Unix epoch string", "[timestamp]") {
    REQUIRE(TimestampUtils::FromZulu("1970-01-01T00:00:00Z") == 0);
}

TEST_CASE("FromZulu parses a known Zulu string", "[timestamp]") {
    REQUIRE(TimestampUtils::FromZulu(REF_ZULU) == REF_TS);
}

TEST_CASE("FromZulu round-trips through ToZulu", "[timestamp]") {
    REQUIRE(TimestampUtils::FromZulu(TimestampUtils::ToZulu(REF_TS)) == REF_TS);
}

TEST_CASE("FromZulu accepts T-with-Z, T-without-Z and space-with-Z formats", "[timestamp]") {
    time_t t1 = TimestampUtils::FromZulu("2024-02-25T12:34:56Z");
    time_t t2 = TimestampUtils::FromZulu("2024-02-25T12:34:56");
    time_t t3 = TimestampUtils::FromZulu("2024-02-25 12:34:56Z");
    REQUIRE(t1 > 0);
    REQUIRE(t1 == t2);
    REQUIRE(t1 == t3);
}

TEST_CASE("FromZulu returns -1 for empty string", "[timestamp]") {
    REQUIRE(TimestampUtils::FromZulu("") == (time_t)-1);
}

TEST_CASE("FromZulu returns -1 for non-date strings", "[timestamp]") {
    REQUIRE(TimestampUtils::FromZulu("not-a-date")        == (time_t)-1);
    REQUIRE(TimestampUtils::FromZulu("2024/02/25 12:34:56") == (time_t)-1);
}

// ── ToLocal ───────────────────────────────────────────────────────────────────

TEST_CASE("ToLocal output has correct YYYY-MM-DD HH:MM:SS shape", "[timestamp]") {
    std::string s = TimestampUtils::ToLocal(REF_TS);
    REQUIRE(s.size() == 19);
    REQUIRE(s[4]  == '-');
    REQUIRE(s[7]  == '-');
    REQUIRE(s[10] == ' ');
    REQUIRE(s[13] == ':');
    REQUIRE(s[16] == ':');
}

// ── FromLocal ─────────────────────────────────────────────────────────────────

TEST_CASE("FromLocal round-trips through ToLocal (timezone-agnostic)", "[timestamp]") {
    // Local formatting depends on the host timezone but must be self-consistent.
    REQUIRE(TimestampUtils::FromLocal(TimestampUtils::ToLocal(REF_TS)) == REF_TS);
}

TEST_CASE("FromLocal returns -1 for empty string", "[timestamp]") {
    REQUIRE(TimestampUtils::FromLocal("") == (time_t)-1);
}

TEST_CASE("FromLocal returns -1 for non-date strings", "[timestamp]") {
    REQUIRE(TimestampUtils::FromLocal("not-a-date") == (time_t)-1);
}
