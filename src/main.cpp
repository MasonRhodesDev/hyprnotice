#include <atomic>
#include <chrono>
#include <csignal>
#include <print>
#include <string_view>

#include <hyprtoolkit/core/Timer.hpp>
#include <hyprutils/memory/Atomic.hpp>

#include <hyprtoolkit/core/Backend.hpp>
#include <sdbus-c++/sdbus-c++.h>

#include "core/NotificationStore.hpp"
#include "core/Theme.hpp"
#include "dbus/InboxService.hpp"
#include "dbus/NotificationsService.hpp"
#include "helpers/Log.hpp"
#include "helpers/Memory.hpp"
#include "ui/PopupManager.hpp"

namespace {
    SP<Hyprtoolkit::IBackend> g_backend;
    std::atomic<bool>         g_reloadRequested{false};

    void onSignal(int sig) {
        if (sig == SIGHUP) {
            // Defer the reload to the main loop — signal context is unsafe
            // for filesystem and hyprtoolkit calls.
            g_reloadRequested.store(true, std::memory_order_relaxed);
            return;
        }
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

    // Load theme before the backend so colors are correct on the first popup.
    HN::g_theme.reload();

    g_backend = Hyprtoolkit::IBackend::create();
    if (!g_backend) {
        std::println(stderr, "failed to create hyprtoolkit backend (Wayland not available?)");
        return 1;
    }

    auto bus = sdbus::createSessionBusConnection();

    HN::CNotificationStore     store;
    HN::CNotificationsService  service(*bus, store);
    HN::CInboxService          inbox(*bus, store, service);
    HN::CPopupManager          popups(g_backend, store, service);

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
    std::signal(SIGHUP, onSignal);

    // Periodic idle check for SIGHUP-triggered theme reloads. hyprtoolkit's
    // addTimer is one-shot, so the callback re-arms itself. Cheap (only
    // re-reads the file when the flag is set). Existing popup color
    // callbacks query g_theme on every paint, so the new palette takes
    // effect on the next frame.
    std::function<void()> armReloadCheck;
    armReloadCheck = [&armReloadCheck] {
        g_backend->addTimer(
            std::chrono::milliseconds(500),
            [&armReloadCheck](Hyprutils::Memory::CAtomicSharedPointer<Hyprtoolkit::CTimer>, void*) {
                if (g_reloadRequested.exchange(false, std::memory_order_relaxed)) {
                    Debug::log(Debug::INFO, "SIGHUP: reloading theme");
                    HN::g_theme.reload();
                }
                armReloadCheck();
            },
            nullptr);
    };
    armReloadCheck();

    g_backend->enterLoop();

    Debug::log(Debug::INFO, "hyprnotice exiting");
    return 0;
}
