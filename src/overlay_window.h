#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#define GLFW_EXPOSE_NATIVE_WIN32

#include <windows.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Transparent, always-on-top, non-focusable overlay window covering the
// primary monitor. Purely visual: the UI lives in ControlWindow.
//
// ClickThrough toggles whether mouse events pass through to the window
// underneath (the game) or are consumed by the overlay.
class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool Ok() const { return window_ != nullptr; }

    HWND        HWnd() const { return hwnd_; }
    GLFWwindow* Glfw() const { return window_; }

    void SetClickThrough(bool enable);
    bool ClickThrough() const { return clickThrough_; }

    bool ShouldClose() const;
    void RequestClose();

    void SwapBuffers();
    void GetFramebufferSize(int& w, int& h) const;

private:
    GLFWwindow* window_       = nullptr;
    HWND        hwnd_         = nullptr;
    bool        clickThrough_ = false;
};
