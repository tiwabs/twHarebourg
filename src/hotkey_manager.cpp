#include "hotkey_manager.h"

bool HotkeyManager::Register(int id, UINT modifiers, UINT vk) {
    if (!RegisterHotKey(hwnd_, id, modifiers, vk))
        return false;
    ids_.push_back(id);
    return true;
}

void HotkeyManager::UnregisterAll() {
    for (int id : ids_)
        UnregisterHotKey(hwnd_, id);
    ids_.clear();
}
