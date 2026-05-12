#include "overlay_window.h"

OverlayWindow::OverlayWindow() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) return;

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) return;

    int mx = 0, my = 0;
    glfwGetMonitorPos(monitor, &mx, &my);

    window_ = glfwCreateWindow(mode->width, mode->height, "twHarebourg overlay", nullptr, nullptr);
    if (!window_) return;

    glfwSetWindowPos(window_, mx, my);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    hwnd_ = glfwGetWin32Window(window_);

    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    ex |=  WS_EX_TOPMOST;
    ex |=  WS_EX_NOACTIVATE;
    ex |=  WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);

    SetWindowPos(hwnd_, HWND_TOPMOST, mx, my, mode->width, mode->height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

OverlayWindow::~OverlayWindow() {
    if (window_) glfwDestroyWindow(window_);
}

void OverlayWindow::SetClickThrough(bool enable) {
    clickThrough_ = enable;
    if (window_)
        glfwSetWindowAttrib(window_, GLFW_MOUSE_PASSTHROUGH, enable ? GLFW_TRUE : GLFW_FALSE);
}

bool OverlayWindow::ShouldClose() const {
    return !window_ || glfwWindowShouldClose(window_);
}

void OverlayWindow::RequestClose() {
    if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void OverlayWindow::SwapBuffers() {
    if (window_) glfwSwapBuffers(window_);
}

void OverlayWindow::GetFramebufferSize(int& w, int& h) const {
    w = h = 0;
    if (window_) glfwGetFramebufferSize(window_, &w, &h);
}
