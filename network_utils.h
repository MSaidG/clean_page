#pragma once

#include <steam/steamnetworkingtypes.h>
#include <string>

void nuke_process(int rc);
void print_usage_and_exit(int rc);

inline bool is_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

inline void ltrim(std::string& s)
{
    s.erase(s.begin(), std::ranges::find_if(s.begin(), s.end(), [](unsigned char c) { return !is_space(c); }));
}

inline void rtrim(std::string& s)
{
    s.erase(std::ranges::find_if(s.rbegin(), s.rend(),
                [](unsigned char c) { return !is_space(c); })
                .base(),
        s.end());
}
