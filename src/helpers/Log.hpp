#pragma once

#include <format>
#include <iostream>
#include <string>
#include <string_view>

namespace Debug {
    enum eLogLevel : uint8_t { TRACE = 0, INFO, LOG, WARN, ERR, CRIT, NONE };

    inline bool quiet   = false;
    inline bool verbose = false;

    template <typename... Args>
    void log(eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (quiet)
            return;
        if (level == TRACE && !verbose)
            return;

        std::string_view prefix;
        switch (level) {
            case TRACE: prefix = "[TRACE]"; break;
            case INFO:  prefix = "[INFO] "; break;
            case LOG:   prefix = "[LOG]  "; break;
            case WARN:  prefix = "[WARN] "; break;
            case ERR:   prefix = "[ERR]  "; break;
            case CRIT:  prefix = "[CRIT] "; break;
            case NONE:  return;
        }

        const auto msg = std::format(fmt, std::forward<Args>(args)...);
        std::cerr << prefix << ' ' << msg << '\n';
    }
}
