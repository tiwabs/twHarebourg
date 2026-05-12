#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#define GLFW_EXPOSE_NATIVE_WIN32

#include <windows.h>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Small always-on-top opaque window that hosts the ImGui control panel.
// Borderless: the ImGui title bar acts as the OS title bar (drag handled
// natively via WM_NCHITTEST returning HTCAPTION, close handled by ImGui's
// built-in close button).
class ControlWindow {
public:
    ControlWindow();
    ~ControlWindow();

    ControlWindow(const ControlWindow&) = delete;
    ControlWindow& operator=(const ControlWindow&) = delete;

    bool Ok() const { return window_ != nullptr; }

    HWND        HWnd() const { return window_ ? glfwGetWin32Window(window_) : nullptr; }
    GLFWwindow* Glfw() const { return window_; }

    bool ShouldClose() const;
    void RequestClose();

    void SwapBuffers();
    void GetFramebufferSize(int& w, int& h) const;
    void GetSize(int& w, int& h) const;
    void SetSize(int w, int h);

private:
    GLFWwindow* window_ = nullptr;
};
