#pragma once

#include <ctime>
#include <string>

// Pure C utility functions for converting between timestamp representations.
// Intentionally wx-free so they can be unit-tested without initialising the
// wxWidgets framework.

namespace TimestampUtils {

    // Parse "YYYY-MM-DD HH:MM:SS" (local time) → time_t.
    // Returns (time_t)-1 on failure.
    time_t FromLocal(const std::string& s);

    // Parse a Zulu/UTC string → time_t.
    // Accepts: YYYY-MM-DDTHH:MM:SSZ  /  YYYY-MM-DDTHH:MM:SS  /  YYYY-MM-DD HH:MM:SSZ
    // Returns (time_t)-1 on failure.
    time_t FromZulu(const std::string& s);

    // Format time_t as "YYYY-MM-DD HH:MM:SS" in the local timezone.
    std::string ToLocal(time_t ts);

    // Format time_t as "YYYY-MM-DDTHH:MM:SSZ" in UTC.
    std::string ToZulu(time_t ts);

} // namespace TimestampUtils
