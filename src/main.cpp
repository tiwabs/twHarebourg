#include "app.h"

#include <GLFW/glfw3.h>
#include <windows.h>

int main() {
    if (!glfwInit()) {
        MessageBoxW(nullptr, L"Failed to initialise GLFW.", L"twHarebourg", MB_ICONERROR);
        return 1;
    }

    int code = 1;
    {
        App app;
        if (!app.Ok()) {
            MessageBoxW(nullptr,
                        L"Failed to initialise the overlay window.",
                        L"twHarebourg",
                        MB_ICONERROR);
        } else {
            code = app.Run();
        }
    }

    glfwTerminate();
    return code;
}
