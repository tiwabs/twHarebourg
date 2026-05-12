#pragma once

#include <windows.h>
#include <vector>

// Thin RAII wrapper around RegisterHotKey / UnregisterHotKey.
//
// Hotkeys registered here are global: they fire even when our overlay window
// doesn't have focus, which is what we want for in-game shortcuts. The
// corresponding WM_HOTKEY messages are sent to the owning HWND and dispatched
// by the App's subclass procedure.
class HotkeyManager {
public:
    explicit HotkeyManager(HWND hwnd) : hwnd_(hwnd) {}
    ~HotkeyManager() { UnregisterAll(); }

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    bool Register(int id, UINT modifiers, UINT vk);
    void UnregisterAll();

private:
    HWND             hwnd_;
    std::vector<int> ids_;
};
