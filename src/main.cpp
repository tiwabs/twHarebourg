#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#define GLFW_EXPOSE_NATIVE_WIN32

#include <windows.h>
#include <commctrl.h>
#include <string>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "grid.h"
#include "logic.h"

#define HOTKEY_PLACEMENT_TOGGLE 1
#define HOTKEY_QUIT   2
#define HOTKEY_INCREASE_MELEE 3
#define HOTKEY_DECREASE_MELEE 4
#define HOTKEY_RESET_MELEE 5
#define HOTKEY_GRID_TOGGLE 6
#define HOTKEY_SAVE 7

static HWND        g_hwnd         = nullptr;
static GLFWwindow* g_window       = nullptr;
static bool        g_clickThrough = false;
static Logic*      g_logic        = nullptr;
static Grid*       g_grid         = nullptr;
static bool        g_gridEdit     = false;

static void RefreshWindowStyle() {
    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );
}

// Toggle mouse pass-through on the overlay window.
// When enabled, all mouse clicks fall through to the window behind the overlay
// so the user can interact with the game normally.
static void SetClickThrough(bool enable) {
    g_clickThrough = enable;
    glfwSetWindowAttrib(
        g_window,
        GLFW_MOUSE_PASSTHROUGH,
        enable ? GLFW_TRUE : GLFW_FALSE
    );
}

// Win32 subclass procedure that intercepts messages for the overlay window.
// It suppresses window activation on mouse clicks (MA_NOACTIVATE) so the
// overlay never steals focus from the game, and it dispatches all registered
// global hotkeys (WM_HOTKEY) to their corresponding actions.
static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_MOUSEACTIVATE: {
            // Prevent the overlay from stealing focus when the user clicks on it.
            return MA_NOACTIVATE;
        }

        case WM_HOTKEY: {
            switch (wParam) {
                case HOTKEY_PLACEMENT_TOGGLE:
                    SetClickThrough(!g_clickThrough);
                    return 0;

                case HOTKEY_QUIT:
                    glfwSetWindowShouldClose(g_window, GLFW_TRUE);
                    return 0;

                case HOTKEY_INCREASE_MELEE:
                    if (g_logic) g_logic->IncMelee();
                    return 0;

                case HOTKEY_DECREASE_MELEE:
                    if (g_logic) g_logic->DecMelee();
                    return 0;

                case HOTKEY_RESET_MELEE:
                    if (g_logic) g_logic->ResetMelee();
                    return 0;

                case HOTKEY_GRID_TOGGLE:
                    g_gridEdit = !g_gridEdit;
                    // Grid editing requires mouse input, so disable click-through
                    // automatically when the user enters grid-edit mode.
                    if (g_gridEdit && g_clickThrough) {
                        SetClickThrough(false);
                    }
                    return 0;

                case HOTKEY_SAVE:
                    if (g_grid) g_grid->SaveToFile("config.json");
                    return 0;

                default:
                    break;
            }
            break;
        }

        default:
            break;
    }

    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

// Register all global hotkeys with the Win32 API.
// Global hotkeys fire even when the overlay window is not focused, which
// means the user can trigger them while the game is in the foreground.
// Returns false if at least one registration failed (e.g. the key is
// already grabbed by another application).
static bool RegisterOverlayHotkeys() {
    struct Binding {
        int  id;
        UINT mods;
        UINT vk;
    };

    const Binding bindings[] = {
        { HOTKEY_PLACEMENT_TOGGLE, MOD_NOREPEAT, VK_F2 },
        { HOTKEY_QUIT,             MOD_NOREPEAT, VK_F10 },
        { HOTKEY_INCREASE_MELEE,   MOD_NOREPEAT, VK_PRIOR },   // PgUp
        { HOTKEY_DECREASE_MELEE,   MOD_NOREPEAT, VK_NEXT },    // PgDwn
        { HOTKEY_RESET_MELEE,      MOD_NOREPEAT, 'R' },
        { HOTKEY_GRID_TOGGLE,      MOD_NOREPEAT, VK_F8 },
        { HOTKEY_SAVE,             MOD_NOREPEAT, VK_F9 },
    };

    bool allOk = true;
    for (const Binding& b : bindings) {
        if (!RegisterHotKey(g_hwnd, b.id, b.mods, b.vk)) {
            allOk = false;
        }
    }
    return allOk;
}

static void UnregisterOverlayHotkeys() {
    const int ids[] = {
        HOTKEY_PLACEMENT_TOGGLE,
        HOTKEY_QUIT,
        HOTKEY_INCREASE_MELEE,
        HOTKEY_DECREASE_MELEE,
        HOTKEY_RESET_MELEE,
        HOTKEY_GRID_TOGGLE,
        HOTKEY_SAVE,
    };
    for (int id : ids) {
        UnregisterHotKey(g_hwnd, id);
    }
}

// Configure the GLFW window as a transparent, always-on-top overlay covering
// the primary monitor. Several Win32 extended-style flags are applied:
//   WS_EX_TOPMOST   — keep the window above all others, including the game
//   WS_EX_NOACTIVATE — never steal keyboard/mouse focus from the game
//   WS_EX_TOOLWINDOW — hide from the taskbar and Alt-Tab list
static void SetupTransparentTopMostWindow(int x, int y, int width, int height) {
    g_hwnd = glfwGetWin32Window(g_window);

    LONG_PTR exStyle = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);

    exStyle |= WS_EX_TOPMOST;

    // Prevent the overlay from becoming the active/focused window.
    exStyle |= WS_EX_NOACTIVATE;

    // Hide in taskbar.
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;

    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        x,
        y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED
    );

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
}

int main() {
    if (!glfwInit()) {
        return 1;
    }

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
    if (!monitor) {
        glfwTerminate();
        return 1;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
        glfwTerminate();
        return 1;
    }

    int monitorX = 0;
    int monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    const int windowWidth = mode->width;
    const int windowHeight = mode->height;

    g_window = glfwCreateWindow(
        windowWidth,
        windowHeight,
        "twHarebourg",
        nullptr,
        nullptr
    );

    if (!g_window) {
        glfwTerminate();
        return 1;
    }

    glfwSetWindowPos(g_window, monitorX, monitorY);
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    SetupTransparentTopMostWindow(
        monitorX,
        monitorY,
        windowWidth,
        windowHeight
    );

    // Verify that the compositor actually supports a transparent framebuffer.
    // Without this, the overlay background will be opaque black instead of
    // fully transparent and the game underneath will be hidden.
    if (!glfwGetWindowAttrib(g_window, GLFW_TRANSPARENT_FRAMEBUFFER)) {
        MessageBoxW(
            nullptr,
            L"GLFW_TRANSPARENT_FRAMEBUFFER is not supported on this system.",
            L"twHarebourg",
            MB_ICONWARNING
        );
    }

    // Attach the Win32 subclass so we can intercept WM_HOTKEY and WM_MOUSEACTIVATE.
    SetWindowSubclass(g_hwnd, SubclassProc, 0, 0);

    if (!RegisterOverlayHotkeys()) {
        MessageBoxW(
            nullptr,
            L"Failed to register one or more hotkeys. F2/F8/F9/F10/PgUp/PgDwn/R may already be used.",
            L"twHarebourg",
            MB_ICONWARNING
        );
    }

    SetClickThrough(false);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Grid grid;
    if (!grid.LoadFromFile("config.json")) {
        MessageBoxW(
            nullptr,
            L"Failed to load config.json.",
            L"twHarebourg",
            MB_ICONWARNING
        );
    }
    g_grid = &grid;

    Logic logic;
    g_logic = &logic;

    while (!glfwWindowShouldClose(g_window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiIO& io = ImGui::GetIO();
        // Only process game-grid clicks when the overlay is in "placement" mode
        // (click-through disabled), grid-edit mode is off, and ImGui itself is
        // not consuming the mouse (e.g., the user is interacting with the HUD).
        if (!g_gridEdit && !g_clickThrough && !io.WantCaptureMouse) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (auto picked = grid.PickCell(io.MousePos)) {
                    logic.player = picked;   // Left-click = set player position
                }
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                if (auto picked = grid.PickCell(io.MousePos)) {
                    logic.target = picked;
                }
            }
        }

        // Track the previous state of each arrow key to detect fresh key-down
        // edges (rising edge detection). This prevents the grid from scrolling
        // at the system key-repeat rate and gives one nudge per press.
        static bool prevArrow[4]   = { false, false, false, false };
        static bool prevGridEdit   = false;
        const int   keys[4]        = { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN };
        const int   dx[4]          = { -1,       1,        0,     0 };
        const int   dy[4]          = {  0,       0,       -1,     1 };

        if (g_gridEdit) {
            const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            // Fine step (1 px) with Shift held, coarse step (4 px) otherwise.
            const int  step  = shift ? 1 : 4;

            for (int i = 0; i < 4; ++i) {
                const bool now = (GetAsyncKeyState(keys[i]) & 0x8000) != 0;
                if (!prevGridEdit) {
                    prevArrow[i] = now;
                    continue;
                }
                // Fire only on the first frame the key is held (rising edge).
                if (now && !prevArrow[i]) {
                    grid.NudgeOrigin(dx[i] * step, dy[i] * step);
                }
                prevArrow[i] = now;
            }

            if (!io.WantCaptureMouse && io.MouseWheel != 0.0f) {
                const int wheelStep = shift ? 1 : 2;
                grid.ResizeTile(io.MouseWheel > 0.0f ? wheelStep : -wheelStep);
            }
        }
        prevGridEdit = g_gridEdit;

        grid.Draw();

        if (logic.player) {
            grid.DrawNoLoSOverlay(*logic.player);
            grid.FillCell(
                logic.player->c, logic.player->r,
                IM_COL32(0, 120, 255, 90),
                IM_COL32(255, 255, 255, 120)
            );
        }
        if (logic.target) {
            grid.FillCell(
                logic.target->c, logic.target->r,
                IM_COL32(255, 0, 0, 90),
                IM_COL32(255, 255, 255, 120)
            );
        }
        const auto suggested = logic.SuggestedCastCell(grid);
        if (suggested) {
            grid.FillCell(
                suggested->c, suggested->r,
                IM_COL32(0, 220, 255, 120),
                IM_COL32(255, 255, 255, 120)
            );
            if (logic.player) {
                grid.DrawCellLine(
                    logic.player->c, logic.player->r,
                    suggested->c, suggested->r,
                    IM_COL32(0, 220, 255, 190), 3.0f
                );
            }
            grid.DrawCellMarker(
                suggested->c, suggested->r,
                IM_COL32(0, 220, 255, 210), nullptr
            );
        }

        ImGui::SetNextWindowSize(ImVec2(440.0f, 280.0f), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Once);

        ImGui::Begin("twHarebourg");

        ImGui::Text("F2 = Mode Placement  |  F10 = quitter");
        ImGui::Text("Mode placement : %s", g_clickThrough ? "OFF" : "ON");
        ImGui::Text("Clic gauche = joueur  |  Clic droit = cible");
        ImGui::Separator();

        ImGui::SliderInt("HP (%)", &logic.hpPercent, 0, 100, "%d %%");
        ImGui::Text("Confusion : %s", logic.ConfusionLabel().c_str());
        ImGui::Separator();

        ImGui::Text("PgUp = +1 melee  |  PgDwn = -1 melee  |  R = Reset melee");
        ImGui::Text("Stacks de melee : %d", logic.meleeStacks);
        ImGui::Separator();

        ImGui::Text("F8 = Mode Edition Grille  |  F9 = Sauvegarder HUD");
        ImGui::Text("Mode Edition : %s", g_gridEdit ? "ON" : "OFF");
        if (g_gridEdit) {
            ImGui::Text("Fleches = deplacer origine  |  Molette = taille tuile");
            ImGui::Text("Maintenir Shift = pas fin");
            ImGui::Text("Origine : (%d, %d)  |  Tuile : %d x %d",
                grid.OriginX(), grid.OriginY(), grid.TileW(), grid.TileH());
        }

        ImGui::Separator();
        if (ImGui::Button("Quitter", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            glfwSetWindowShouldClose(g_window, GLFW_TRUE);
        }

        ImGui::End();

        ImGui::Render();

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(
            g_window,
            &framebufferWidth,
            &framebufferHeight
        );

        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // Clear to fully transparent black so the game is visible through the overlay.
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g_window);
    }

    g_logic = nullptr;
    g_grid  = nullptr;

    UnregisterOverlayHotkeys();
    RemoveWindowSubclass(g_hwnd, SubclassProc, 0);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_window);
    glfwTerminate();

    return 0;
}
