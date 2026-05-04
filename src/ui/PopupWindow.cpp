#include "PopupWindow.hpp"

#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprutils/memory/Atomic.hpp>

#include "../core/NotificationStore.hpp"
#include "../core/Theme.hpp"
#include "../dbus/NotificationsService.hpp"
#include "../helpers/Log.hpp"
#include "LayerShell.hpp"

namespace HN {

    using namespace Hyprtoolkit;

    namespace {
        constexpr int32_t  kDefaultTimeoutMs = 5000;
        constexpr Hyprutils::Math::Vector2D kPopupSize{360, 110};
        constexpr int      kEdgeMargin       = 12;
    }

    CPopupWindow::CPopupWindow(SP<IBackend> backend, SP<SNotification> notification, CNotificationStore& store, CNotificationsService& notif)
        : m_backend(backend), m_notif(notification), m_store(store), m_notifService(notif), m_id(notification->id) {

        m_window = CWindowBuilder::begin()
                       ->type(HT_WINDOW_LAYER)
                       ->preferredSize(kPopupSize)
                       ->appClass("hyprnotice")
                       ->appTitle(std::format("Notification {}", m_id))
                       ->layer(LayerShell::LAYER_OVERLAY)
                       ->anchor(LayerShell::ANCHOR_TOP | LayerShell::ANCHOR_RIGHT)
                       ->marginTopLeft({kEdgeMargin, kEdgeMargin})
                       ->kbInteractive(LayerShell::KB_NONE)
                       ->commence();

        buildContent();

        m_window->open();

        const auto timeoutMs = m_notif->expireTimeoutMs > 0 ? m_notif->expireTimeoutMs : kDefaultTimeoutMs;
        // 0 = persistent (spec); negative = server default. expireTimeoutMs == 0
        // skips the auto-close so the popup stays until explicitly dismissed.
        if (m_notif->expireTimeoutMs != 0)
            scheduleAutoClose(timeoutMs);
    }

    CPopupWindow::~CPopupWindow() {
        if (m_autoCloseTimer)
            m_autoCloseTimer->cancel();
        if (m_window)
            m_window->close();
    }

    void CPopupWindow::buildContent() {
        // Theme colors: prefer the matugen-generated lmtt palette, fall back
        // to hyprtoolkit's built-in palette so the popup is still visible
        // before lmtt has run for the first time. Lambdas capture nothing
        // expensive — they're called once per element rebuild.
        auto bg = m_backend;
        const auto bgFn = [bg] {
            return g_theme.get("surface_container", bg->getPalette()->m_colors.background);
        };
        const auto textFn = [bg] {
            return g_theme.get("on_surface", bg->getPalette()->m_colors.text);
        };

        m_window->m_rootElement->addChild(
            CRectangleBuilder::begin()
                ->color(bgFn)
                ->rounding(10)
                ->commence());

        auto col = CColumnLayoutBuilder::begin()
                       ->gap(6)
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();
        m_window->m_rootElement->addChild(col);

        auto title = CTextBuilder::begin()
                         ->text(std::format("{}: {}",
                                            m_notif->appName.empty() ? "notification" : m_notif->appName,
                                            m_notif->summary))
                         ->fontSize(CFontSize{CFontSize::HT_FONT_H3})
                         ->color(textFn)
                         ->commence();
        col->addChild(title);

        if (!m_notif->body.empty()) {
            auto body = CTextBuilder::begin()
                            ->text(std::string{m_notif->body})
                            ->fontSize(CFontSize{CFontSize::HT_FONT_TEXT})
                            ->color(textFn)
                            ->noEllipsize(false)
                            ->commence();
            col->addChild(body);
        }

        // Action buttons. notification.actions is alternating [key, label, ...].
        // The "default" action key is the click-on-the-popup action and gets
        // no button — it's invoked by clicking the popup body itself (not yet
        // wired; tracked for v0.4). All other named actions become buttons.
        if (m_notif->actions.size() >= 2) {
            auto row = CRowLayoutBuilder::begin()
                           ->gap(6)
                           ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                           ->commence();

            const auto id     = m_id;
            auto&      store  = m_store;
            auto&      svc    = m_notifService;

            for (size_t i = 0; i + 1 < m_notif->actions.size(); i += 2) {
                const auto& key   = m_notif->actions[i];
                const auto& label = m_notif->actions[i + 1];
                if (key == "default")
                    continue;
                auto btn = CButtonBuilder::begin()
                               ->label(std::string{label})
                               ->onMainClick([id, key, &svc, &store](SP<CButtonElement>) {
                                   svc.emitActionInvoked(id, key);
                                   store.close(id, SNotification::eCloseReason::DISMISSED);
                               })
                               ->size({CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {1, 1}})
                               ->commence();
                row->addChild(btn);
            }
            col->addChild(row);
        }
    }

    void CPopupWindow::scheduleAutoClose(int32_t ms) {
        const auto id    = m_id;
        auto&      store = m_store;
        m_autoCloseTimer = m_backend->addTimer(
            std::chrono::milliseconds(ms),
            [id, &store](Hyprutils::Memory::CAtomicSharedPointer<CTimer>, void*) {
                // Demote, don't close — preserves freedesktop action handlers
                // on the sender so the inbox can still invoke them later.
                store.demote(id);
            },
            nullptr);
    }

    void CPopupWindow::rebuild(SP<SNotification> updated) {
        m_notif = updated;
        // Tearing the window down and re-opening is simpler than mutating the
        // existing element tree in place. Replaces are rare enough that the
        // perf hit is irrelevant; correctness wins.
        if (m_window)
            m_window->close();
        m_window = CWindowBuilder::begin()
                       ->type(HT_WINDOW_LAYER)
                       ->preferredSize(kPopupSize)
                       ->appClass("hyprnotice")
                       ->appTitle(std::format("Notification {}", m_id))
                       ->layer(LayerShell::LAYER_OVERLAY)
                       ->anchor(LayerShell::ANCHOR_TOP | LayerShell::ANCHOR_RIGHT)
                       ->marginTopLeft({kEdgeMargin, kEdgeMargin})
                       ->kbInteractive(LayerShell::KB_NONE)
                       ->commence();
        buildContent();
        m_window->open();

        if (m_autoCloseTimer)
            m_autoCloseTimer->cancel();
        if (m_notif->expireTimeoutMs != 0) {
            scheduleAutoClose(m_notif->expireTimeoutMs > 0 ? m_notif->expireTimeoutMs : kDefaultTimeoutMs);
        }
    }

    void CPopupWindow::close() {
        if (m_autoCloseTimer) {
            m_autoCloseTimer->cancel();
            m_autoCloseTimer.reset();
        }
        if (m_window) {
            m_window->close();
            m_window.reset();
        }
    }

}
