// hyprnotice-ctl: thin CLI for the hyprnotice daemon's inbox.
//
// All commands talk to org.freedesktop.Notifications on the user session bus,
// hitting the custom org.hyprnotice.Inbox interface at /org/hyprnotice/Inbox.

#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

namespace {
    constexpr const char* kBus       = "org.freedesktop.Notifications";
    constexpr const char* kPath      = "/org/hyprnotice/Inbox";
    constexpr const char* kInterface = "org.hyprnotice.Inbox";

    void usage() {
        std::println(stderr,
            "Usage: hyprnotice-ctl <command> [args...]\n"
            "\n"
            "Commands:\n"
            "  list                       List all notifications (visible + inbox)\n"
            "  invoke <id> [action_key]   Invoke an action on the notification\n"
            "                             (default: \"default\")\n"
            "  dismiss <id>               Close a single notification\n"
            "  dismiss-all                Close every notification");
    }

    int doList(sdbus::IProxy& proxy) {
        std::vector<sdbus::Struct<uint32_t, std::string, std::string, std::string, std::string>> rows;
        proxy.callMethod("List").onInterface(kInterface).storeResultsTo(rows);
        if (rows.empty()) {
            std::println("(empty)");
            return 0;
        }
        for (const auto& r : rows) {
            const auto id    = std::get<0>(r);
            const auto& app  = std::get<1>(r);
            const auto& sum  = std::get<2>(r);
            const auto& body = std::get<3>(r);
            const auto& st   = std::get<4>(r);
            if (body.empty())
                std::println("[{}] #{:>3} ({}) {}", st, id, app, sum);
            else
                std::println("[{}] #{:>3} ({}) {} — {}", st, id, app, sum, body);
        }
        return 0;
    }

    int doInvoke(sdbus::IProxy& proxy, uint32_t id, const std::string& key) {
        proxy.callMethod("Invoke").onInterface(kInterface).withArguments(id, key);
        return 0;
    }

    int doDismiss(sdbus::IProxy& proxy, uint32_t id) {
        proxy.callMethod("Dismiss").onInterface(kInterface).withArguments(id);
        return 0;
    }

    int doDismissAll(sdbus::IProxy& proxy) {
        proxy.callMethod("DismissAll").onInterface(kInterface);
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::string_view cmd{argv[1]};

    auto bus   = sdbus::createSessionBusConnection();
    auto proxy = sdbus::createProxy(*bus, sdbus::ServiceName{kBus}, sdbus::ObjectPath{kPath});

    try {
        if (cmd == "list")
            return doList(*proxy);
        if (cmd == "dismiss-all")
            return doDismissAll(*proxy);
        if (cmd == "dismiss") {
            if (argc < 3) {
                std::println(stderr, "dismiss: missing <id>");
                return 2;
            }
            return doDismiss(*proxy, static_cast<uint32_t>(std::stoul(argv[2])));
        }
        if (cmd == "invoke") {
            if (argc < 3) {
                std::println(stderr, "invoke: missing <id>");
                return 2;
            }
            const auto        id  = static_cast<uint32_t>(std::stoul(argv[2]));
            const std::string key = argc > 3 ? argv[3] : "default";
            return doInvoke(*proxy, id, key);
        }
    } catch (const sdbus::Error& e) {
        std::println(stderr, "D-Bus error: {}: {}", e.getName().c_str(), e.getMessage());
        return 1;
    } catch (const std::exception& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }

    std::println(stderr, "unknown command: {}", cmd);
    usage();
    return 2;
}
