#include "PopupWindow.hpp"

#include <filesystem>

#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/system/Icons.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprutils/memory/Atomic.hpp>

#include "../core/Config.hpp"
#include "../core/NotificationStore.hpp"
#include "../core/Theme.hpp"
#include "../dbus/NotificationsService.hpp"
#include "../helpers/Log.hpp"
#include "LayerShell.hpp"

namespace HN {

    using namespace Hyprtoolkit;

    namespace {
        // Build a fresh CWindowBuilder for a notification popup using the
        // current hyprlang config snapshot. Centralised here so the ctor and
        // rebuild() path don't drift.
        SP<IWindow> makeWindow(uint32_t id) {
            const auto& cfg = g_config.current();
            return CWindowBuilder::begin()
                ->type(HT_WINDOW_LAYER)
                ->preferredSize({(double)cfg.width, (double)cfg.height})
                ->appClass("hyprnotice")
                ->appTitle(std::format("Notification {}", id))
                ->layer(cfg.layer)
                ->anchor(cfg.anchor)
                ->marginTopLeft({(double)cfg.margin_left, (double)cfg.margin_top})
                ->kbInteractive(LayerShell::KB_NONE)
                ->commence();
        }
    }

    CPopupWindow::CPopupWindow(SP<IBackend> backend, SP<SNotification> notification, CNotificationStore& store, CNotificationsService& notif)
        : m_backend(backend), m_notif(notification), m_store(store), m_notifService(notif), m_id(notification->id) {

        m_window = makeWindow(m_id);
        buildContent();
        m_window->open();

        // 0 = persistent (spec); negative = server default. We treat negative
        // as "use config default_timeout", and zero as "never auto-close".
        if (m_notif->expireTimeoutMs != 0) {
            const auto ms = m_notif->expireTimeoutMs > 0
                                ? m_notif->expireTimeoutMs
                                : g_config.current().default_timeout;
            if (ms > 0)
                scheduleAutoClose(ms);
        }
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

        // Top-level row: [icon] [text column]. If no icon was provided we
        // skip the icon child and let the column expand to full width.
        auto row = CRowLayoutBuilder::begin()
                       ->gap(10)
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();
        m_window->m_rootElement->addChild(row);

        if (!m_notif->appIcon.empty()) {
            constexpr Hyprutils::Math::Vector2D kIconSize{48, 48};
            const auto& icon = m_notif->appIcon;
            // Heuristic: leading "/" or "file://" → absolute path; otherwise
            // freedesktop icon name (resolved via hyprtoolkit's systemIcons
            // factory which walks the user's icon-theme search path).
            SP<CImageElement> img;
            if (icon.starts_with("/") || icon.starts_with("file://")) {
                std::string path = icon.starts_with("file://") ? icon.substr(7) : icon;
                if (std::filesystem::exists(path)) {
                    img = CImageBuilder::begin()
                              ->path(std::move(path))
                              ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, kIconSize})
                              ->rounding(6)
                              ->commence();
                }
            } else if (auto desc = m_backend->systemIcons()->lookupIcon(icon)) {
                img = CImageBuilder::begin()
                          ->icon(desc)
                          ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, kIconSize})
                          ->commence();
            }
            if (img)
                row->addChild(img);
        }

        auto col = CColumnLayoutBuilder::begin()
                       ->gap(6)
                       ->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {1.F, 1.F}})
                       ->commence();
        row->addChild(col);

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
                // Render every action including "default" — the latter
                // typically carries a label like "Open" and shows up alongside
                // any named actions. Body-click handling for "default" is
                // tracked for v0.6 (hyprtoolkit's IElement doesn't expose a
                // click-anywhere hook yet).
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
        m_window = makeWindow(m_id);
        buildContent();
        m_window->open();

        if (m_autoCloseTimer)
            m_autoCloseTimer->cancel();
        if (m_notif->expireTimeoutMs != 0) {
            const auto ms = m_notif->expireTimeoutMs > 0
                                ? m_notif->expireTimeoutMs
                                : g_config.current().default_timeout;
            if (ms > 0)
                scheduleAutoClose(ms);
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
