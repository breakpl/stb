#include "TimestampUtils.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#  define timegm _mkgmtime
#endif

namespace TimestampUtils {

time_t FromLocal(const std::string& s) {
    int y, mo, d, h, mi, sec;
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6)
        return (time_t)-1;
    struct tm t = {};
    t.tm_year  = y - 1900;
    t.tm_mon   = mo - 1;
    t.tm_mday  = d;
    t.tm_hour  = h;
    t.tm_min   = mi;
    t.tm_sec   = sec;
    t.tm_isdst = -1;
    return mktime(&t);
}

time_t FromZulu(const std::string& s) {
    int y, mo, d, h, mi, sec;
    bool ok = (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &mo, &d, &h, &mi, &sec) == 6)
           || (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d",  &y, &mo, &d, &h, &mi, &sec) == 6)
           || (sscanf(s.c_str(), "%d-%d-%d %d:%d:%dZ", &y, &mo, &d, &h, &mi, &sec) == 6);
    if (!ok) return (time_t)-1;
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon  = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min  = mi;
    t.tm_sec  = sec;
    return timegm(&t);
}

std::string ToLocal(time_t ts) {
    char buf[32];
    struct tm* lt = localtime(&ts);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
    return buf;
}

std::string ToZulu(time_t ts) {
    char buf[32];
    struct tm* ut = gmtime(&ts);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", ut);
    return buf;
}

} // namespace TimestampUtils
