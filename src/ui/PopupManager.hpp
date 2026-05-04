#pragma once

#include <unordered_map>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprutils/signal/Signal.hpp>

#include "PopupWindow.hpp"

namespace HN {

    class CNotificationStore;

    // Bridges CNotificationStore signals → on-screen layer-shell popups. One
    // CPopupWindow per VISIBLE notification; INBOX/CLOSED entries don't have
    // a window. The manager subscribes once and keeps its listeners alive for
    // its full lifetime.
    class CPopupManager {
      public:
        CPopupManager(SP<Hyprtoolkit::IBackend> backend, CNotificationStore& store);
        ~CPopupManager();

      private:
        void onAdded(SP<SNotification> n);
        void onUpdated(SP<SNotification> n);
        void onClosed(uint32_t id, SNotification::eCloseReason reason);

        SP<Hyprtoolkit::IBackend>                                  m_backend;
        CNotificationStore&                                        m_store;
        std::unordered_map<uint32_t, UP<CPopupWindow>>             m_popups;

        Hyprutils::Signal::CHyprSignalListener                     m_addedListener;
        Hyprutils::Signal::CHyprSignalListener                     m_updatedListener;
        Hyprutils::Signal::CHyprSignalListener                     m_closedListener;
    };

}
