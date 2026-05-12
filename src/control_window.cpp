#include "control_window.h"

ControlWindow::ControlWindow() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    // Initial size is just a placeholder — App resizes to fit the ImGui content
    // every frame.
    window_ = glfwCreateWindow(330, 120, "twHarebourg", nullptr, nullptr);
    if (!window_) return;

    glfwSetWindowPos(window_, 40, 40);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
}

ControlWindow::~ControlWindow() {
    if (window_) glfwDestroyWindow(window_);
}

bool ControlWindow::ShouldClose() const {
    return !window_ || glfwWindowShouldClose(window_);
}

void ControlWindow::RequestClose() {
    if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

void ControlWindow::SwapBuffers() {
    if (window_) glfwSwapBuffers(window_);
}

void ControlWindow::GetFramebufferSize(int& w, int& h) const {
    w = h = 0;
    if (window_) glfwGetFramebufferSize(window_, &w, &h);
}

void ControlWindow::GetSize(int& w, int& h) const {
    w = h = 0;
    if (window_) glfwGetWindowSize(window_, &w, &h);
}

void ControlWindow::SetSize(int w, int h) {
    if (window_) glfwSetWindowSize(window_, w, h);
}
