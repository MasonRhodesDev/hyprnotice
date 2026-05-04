#include <csignal>
#include <print>
#include <string_view>

#include <hyprtoolkit/core/Backend.hpp>
#include <sdbus-c++/sdbus-c++.h>

#include "core/NotificationStore.hpp"
#include "dbus/NotificationsService.hpp"
#include "helpers/Log.hpp"
#include "helpers/Memory.hpp"
#include "ui/PopupManager.hpp"

namespace {
    SP<Hyprtoolkit::IBackend> g_backend;

    void onSignal(int) {
        if (g_backend)
            g_backend->destroy();
    }

    void printUsage() {
        std::println(stderr,
            "hyprnotice {} — notification daemon for Hyprland\n"
            "\n"
            "Usage: hyprnotice [--version] [--help] [--verbose] [--quiet]\n"
            "\n"
            "Implements org.freedesktop.Notifications on the user session bus,\n"
            "renders popups via hyprtoolkit layer-shell windows.\n",
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

    g_backend = Hyprtoolkit::IBackend::create();
    if (!g_backend) {
        std::println(stderr, "failed to create hyprtoolkit backend (Wayland not available?)");
        return 1;
    }

    auto bus = sdbus::createSessionBusConnection();

    HN::CNotificationStore     store;
    HN::CNotificationsService  service(*bus, store);
    HN::CPopupManager          popups(g_backend, store);

    // Drive sdbus's event loop from inside hyprtoolkit's. sdbus exposes a
    // single fd we can poll; on read-ready we drain pending requests. The
    // timeout half of sdbus's PollData is unused for now — adequate for a
    // server-side daemon (we only originate replies, not method calls).
    const auto poll = bus->getEventLoopPollData();
    g_backend->addFd(poll.fd, [&bus] {
        while (bus->processPendingEvent()) { /* drain */ }
    });

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    g_backend->enterLoop();

    Debug::log(Debug::INFO, "hyprnotice exiting");
    return 0;
}
