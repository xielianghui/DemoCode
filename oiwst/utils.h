#pragma once
#include "time.h"
#include <string>

// time utils
namespace timeutils
{
    static tm* current_date_time(tm* result, bool is2human = true)
    {
        time_t time_seconds;
        time(&time_seconds);
#ifdef _WIN32
        localtime_s(result, &time_seconds);
#else
        localtime_r(&time_seconds, result);
#endif
        if (is2human) {
            result->tm_year = (result->tm_year + 1900);
            result->tm_mon = (result->tm_mon + 1);
        }
        return result;
    }

    static std::string CurDateStr()
    {
        struct tm local;
        current_date_time(&local);
        char sbuffer[16] = { 0 };
        snprintf(sbuffer, sizeof(sbuffer), "%4d%02d%02d", local.tm_year, local.tm_mon, local.tm_mday);
        return std::move(std::string(sbuffer));
    }

    static std::string CurTimeStr(char delimiter = ':')
    {
        struct tm local;
        current_date_time(&local);
        char sbuffer[16] = { 0 };
        snprintf(sbuffer, sizeof(sbuffer), "%02d%c%02d%c%02d", local.tm_hour, delimiter, local.tm_min, delimiter, local.tm_sec);
        return std::move(std::string(sbuffer));
    }

    static int CurTime(const int base = 100)
    {
        struct tm local;
        current_date_time(&local);
        return (local.tm_hour * base + local.tm_min) * base + local.tm_sec;
    }
}


