#include <csignal>
#include <print>
#include <string_view>

#include <sdbus-c++/sdbus-c++.h>

#include "core/NotificationStore.hpp"
#include "dbus/NotificationsService.hpp"
#include "helpers/Log.hpp"
#include "helpers/Memory.hpp"

namespace {
    sdbus::IConnection* g_busPtr = nullptr;

    void onSignal(int) {
        if (g_busPtr)
            g_busPtr->leaveEventLoop();
    }

    void printUsage() {
        std::println(stderr,
            "hyprnotice {} — notification daemon for Hyprland\n"
            "\n"
            "Usage: hyprnotice [--version] [--help] [--verbose] [--quiet]\n"
            "\n"
            "Implements org.freedesktop.Notifications on the user session bus.\n",
            HYPRNOTICE_VERSION);
    }
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view a{argv[i]};
        if (a == "--version" || a == "-v") {
            std::println("{}", HYPRNOTICE_VERSION);
            return 0;
        }
        if (a == "--help" || a == "-h") {
            printUsage();
            return 0;
        }
        if (a == "--verbose")
            Debug::verbose = true;
        else if (a == "--quiet")
            Debug::quiet = true;
        else {
            std::println(stderr, "unknown argument: {}", a);
            printUsage();
            return 1;
        }
    }

    Debug::log(Debug::INFO, "hyprnotice {} starting", HYPRNOTICE_VERSION);

    auto bus = sdbus::createSessionBusConnection();
    g_busPtr = bus.get();

    HN::CNotificationStore     store;
    HN::CNotificationsService  service(*bus, store);

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    bus->enterEventLoop();

    Debug::log(Debug::INFO, "hyprnotice exiting");
    return 0;
}
