#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#define GLFW_EXPOSE_NATIVE_WIN32

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

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
static bool        g_drawMode     = false;
static Grid::GridSnapshot g_gridSnapshot = {};  // snapshot pris a l'entree en mode dessin

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

// Low-level keyboard hook used in draw mode to feed key events into ImGui
// without activating the overlay window (which would break transparency).
static HHOOK g_keyboardHook = nullptr;

static ImGuiKey VkToImGuiKey(DWORD vk) {
    switch (vk) {
    case VK_BACK:     return ImGuiKey_Backspace;
    case VK_DELETE:   return ImGuiKey_Delete;
    case VK_LEFT:     return ImGuiKey_LeftArrow;
    case VK_RIGHT:    return ImGuiKey_RightArrow;
    case VK_UP:       return ImGuiKey_UpArrow;
    case VK_DOWN:     return ImGuiKey_DownArrow;
    case VK_HOME:     return ImGuiKey_Home;
    case VK_END:      return ImGuiKey_End;
    case VK_RETURN:   return ImGuiKey_Enter;
    case VK_ESCAPE:   return ImGuiKey_Escape;
    case VK_LCONTROL: case VK_RCONTROL: return ImGuiKey_LeftCtrl;
    case VK_LSHIFT:   case VK_RSHIFT:   return ImGuiKey_LeftShift;
    case VK_LMENU:    case VK_RMENU:    return ImGuiKey_LeftAlt;
    case 'A': return ImGuiKey_A;
    case 'C': return ImGuiKey_C;
    case 'V': return ImGuiKey_V;
    case 'X': return ImGuiKey_X;
    case 'Z': return ImGuiKey_Z;
    default:  return ImGuiKey_None;
    }
}

static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_drawMode) {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
        const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        const bool isUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);
        ImGuiIO& io = ImGui::GetIO();

        if (isDown || isUp) {
            // Inject special/modifier key state
            const ImGuiKey imKey = VkToImGuiKey(kb->vkCode);
            if (imKey != ImGuiKey_None)
                io.AddKeyEvent(imKey, isDown);

            // Inject printable characters using the current keyboard layout
            if (isDown) {
                BYTE ks[256] = {};
                GetKeyboardState(ks);
                // Ensure modifier states are up-to-date (LL hook precedes state update)
                if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) ks[VK_SHIFT]   = 0x80;
                if (GetAsyncKeyState(VK_CONTROL) & 0x8000) ks[VK_CONTROL] = 0x80;
                if (GetKeyState(VK_CAPITAL)      & 0x0001) ks[VK_CAPITAL] = 0x01;
                WCHAR chars[4] = {};
                const int n = ToUnicodeEx(
                    kb->vkCode, kb->scanCode, ks,
                    chars, 4, 0, GetKeyboardLayout(0));
                if (n > 0)
                    for (int i = 0; i < n; i++)
                        io.AddInputCharacterUTF16(chars[i]);
            }

            // Block key from reaching other windows (game) while typing
            if (io.WantTextInput)
                return 1;
        }
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

// Enable/disable draw mode.
// Uses a WH_KEYBOARD_LL hook to feed keyboard input into ImGui without ever
// activating the overlay window, so the transparent framebuffer is preserved.
static void SetDrawMode(bool enable) {
    g_drawMode = enable;
    if (enable) {
        // Save snapshot for potential cancel
        if (g_grid) g_gridSnapshot = g_grid->GetGridSnapshot();
        g_keyboardHook = SetWindowsHookEx(
            WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(nullptr), 0);
    } else {
        if (g_keyboardHook) {
            UnhookWindowsHookEx(g_keyboardHook);
            g_keyboardHook = nullptr;
        }
    }
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
                    if (g_gridEdit && g_clickThrough) {
                        SetClickThrough(false);
                    }
                    if (g_gridEdit) SetDrawMode(false);
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
        { HOTKEY_RESET_MELEE,      MOD_NOREPEAT, VK_DELETE },
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
        // (click-through disabled), grid-edit mode is off, draw mode is off,
        // and ImGui itself is not consuming the mouse.
        if (!g_drawMode && !g_gridEdit && !g_clickThrough && !io.WantCaptureMouse) {
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

        // Draw mode: paint cells by clicking or dragging on the isometric grid.
        // Left-click/drag: on press, cycle the first cell (0→1→2→0) and lock that
        //   target type for the whole drag so all painted cells get the same value.
        // Right-click/drag: erase (set to 0) all cells under the cursor.
        if (g_drawMode && !g_clickThrough && !io.WantCaptureMouse) {
            static int  s_dragType    = -1; // target type for current left-drag (-1 = not dragging)
            static Cell s_lastPainted = {-1, -1}; // avoid repainting the same cell twice in one drag

            // Left button — start or continue drag
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (auto picked = grid.PickAnyCell(io.MousePos)) {
                    const int current = grid.GetCellType(picked->c, picked->r);
                    s_dragType    = (current + 1) % 3;
                    s_lastPainted = *picked;
                    grid.SetCellType(picked->c, picked->r, s_dragType);
                }
            } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && s_dragType >= 0) {
                if (auto picked = grid.PickAnyCell(io.MousePos)) {
                    if (*picked != s_lastPainted) {
                        grid.SetCellType(picked->c, picked->r, s_dragType);
                        s_lastPainted = *picked;
                    }
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                s_dragType    = -1;
                s_lastPainted = {-1, -1};
            }

            // Right button — erase on click or hold
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                if (auto picked = grid.PickAnyCell(io.MousePos)) {
                    grid.SetCellType(picked->c, picked->r, 0);
                }
            }

            // Hover preview: highlight the cell under the cursor to show
            // what type it would become after a left-click.
            if (auto hovered = grid.PickAnyCell(io.MousePos)) {
                grid.DrawHoverPreview(*hovered);
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

        if (g_drawMode) {
            grid.DrawEditOverlay();
        }

        // Hover highlight in placement mode — rendered after grid.Draw() so it
        // appears on top of the base grid.
        if (!g_drawMode && !g_gridEdit && !g_clickThrough && !io.WantCaptureMouse) {
            if (auto hovered = grid.PickCell(io.MousePos)) {
                grid.FillCell(
                    hovered->c, hovered->r,
                    IM_COL32(255, 255, 255, 40),
                    IM_COL32(255, 255, 255, 140)
                );
            }
        }

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

        ImGui::SetNextWindowSize(ImVec2(310.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Once);

        ImGui::Begin("twHarebourg");

        if (g_drawMode) {
            // ================================================================
            // MODE DESSIN
            // ================================================================

            // Map name (InputText sans label)
            {
                static int  lastMapIdx = -1;
                static char nameBuf[128] = {};
                const int   curIdx = grid.ActiveMapIndex();
                if (lastMapIdx != curIdx) {
                    snprintf(nameBuf, sizeof(nameBuf), "%s", grid.MapName(curIdx).c_str());
                    lastMapIdx = curIdx;
                }
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::InputTextWithHint("##mapname", "Nom de la map", nameBuf, sizeof(nameBuf)))
                    grid.RenameMap(curIdx, nameBuf);
            }

            // Grid size
            {
                static int resizeRowsBuf = 0;
                static int resizeColsBuf = 0;
                static int resizeLastIdx = -1;
                const int  curIdx = grid.ActiveMapIndex();
                if (resizeLastIdx != curIdx) {
                    resizeRowsBuf = grid.Rows();
                    resizeColsBuf = grid.Cols();
                    resizeLastIdx = curIdx;
                }
                const float inputW = 50.0f;
                ImGui::Text("Lignes");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputW);
                if (ImGui::InputInt("##rows", &resizeRowsBuf, 0, 0)) {
                    if (resizeRowsBuf < 1)  resizeRowsBuf = 1;
                    if (resizeRowsBuf > 60) resizeRowsBuf = 60;
                    grid.ResizeGrid(resizeRowsBuf, resizeColsBuf);
                }
                ImGui::SameLine();
                ImGui::Text("Cols");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(inputW);
                if (ImGui::InputInt("##cols", &resizeColsBuf, 0, 0)) {
                    if (resizeColsBuf < 1)  resizeColsBuf = 1;
                    if (resizeColsBuf > 60) resizeColsBuf = 60;
                    grid.ResizeGrid(resizeRowsBuf, resizeColsBuf);
                }
            }

            ImGui::Separator();

            {
                const float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 130, 60, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 170, 80, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 110, 50, 255));
                if (ImGui::Button("(F9) Sauvegarder", ImVec2(halfW, 0.0f))) {
                    grid.SaveToFile("config.json");
                    SetDrawMode(false);
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine();
                if (ImGui::Button("Annuler", ImVec2(halfW, 0.0f))) {
                    grid.RestoreGridSnapshot(g_gridSnapshot);
                    SetDrawMode(false);
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("G: vide > walkable > LoS    D: effacer");

            ImGui::Separator();
            if (ImGui::Button("(F10) Quitter", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                glfwSetWindowShouldClose(g_window, GLFW_TRUE);

        } else {
            // ================================================================
            // MODE NORMAL
            // ================================================================

            // Map selector + gestion
            {
                const int mapCount = grid.MapCount();
                if (mapCount > 0) {
                    std::vector<const char*> names;
                    names.reserve(mapCount);
                    for (int i = 0; i < mapCount; ++i)
                        names.push_back(grid.MapName(i).c_str());

                    int sel = grid.ActiveMapIndex();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::Combo("##mapsel", &sel, names.data(), mapCount)) {
                        if (grid.SwitchToMap(sel)) {
                            logic.player = std::nullopt;
                            logic.target = std::nullopt;
                        }
                    }
                }

                const float halfW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ImGui::Button("Editer", ImVec2(halfW, 0.0f))) {
                    if (g_clickThrough) SetClickThrough(false);
                    g_gridEdit = false;
                    SetDrawMode(true);
                }
                ImGui::SameLine();
                if (ImGui::Button("+ Nouvelle", ImVec2(halfW, 0.0f))) {
                    const int rows = grid.Rows() > 0 ? grid.Rows() : 20;
                    const int cols = grid.Cols() > 0 ? grid.Cols() : 20;
                    // Snapshot AVANT d'ajouter la map, pour que Annuler puisse la supprimer
                    g_gridSnapshot = grid.GetGridSnapshot();
                    grid.AddMap("Nouvelle map", rows, cols);
                    grid.SwitchToMap(grid.MapCount() - 1);
                    logic.player = std::nullopt;
                    logic.target = std::nullopt;
                    if (g_clickThrough) SetClickThrough(false);
                    g_gridEdit = false;
                    g_drawMode = true;  // SetDrawMode sans écraser le snapshot
                    if (g_keyboardHook == nullptr)
                        g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(nullptr), 0);
                }
                if (grid.MapCount() > 1) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(120, 30, 30, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(170, 50, 50, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(190, 60, 60, 255));
                    if (ImGui::Button("Supprimer cette map", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                        const int idx = grid.ActiveMapIndex();
                        grid.DeleteMap(idx);
                        grid.SwitchToMap(std::max(0, idx - 1));
                        logic.player = std::nullopt;
                        logic.target = std::nullopt;
                        grid.SaveToFile("config.json");
                    }
                    ImGui::PopStyleColor(3);
                }
            }

            ImGui::Separator();

            // Bouton placement (vert = actif, gris = inactif)
            {
                const bool on = !g_clickThrough;
                if (on) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(40, 130, 60, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 170, 80, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(30, 110, 50, 255));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(65, 65, 65, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 90, 90, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(50, 50, 50, 255));
                }
                if (ImGui::Button(on ? "(F2) Placement : ON" : "(F2) Placement : OFF",
                                  ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
                    SetClickThrough(!g_clickThrough);
                ImGui::PopStyleColor(3);
                if (!g_clickThrough)
                    ImGui::TextDisabled("G = joueur   D = cible");
            }

            ImGui::Separator();

            // HP + Confusion
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::SliderInt("##hp", &logic.hpPercent, 0, 100, "HP : %d%%");
            ImGui::Text("Confusion : %s", logic.ConfusionLabel().c_str());

            ImGui::Separator();

            // Melee stacks avec boutons inline
            {
                ImGui::Text("Melee :");
                ImGui::SameLine();
                if (ImGui::SmallButton("(PgDn)-##mel"))  logic.DecMelee();
                ImGui::SameLine();
                ImGui::Text("%d", logic.meleeStacks);
                ImGui::SameLine();
                if (ImGui::SmallButton("+(PgUp)##mel"))  logic.IncMelee();
                ImGui::SameLine();
                if (ImGui::SmallButton("x(Suppr)##mel"))  logic.ResetMelee();
            }

            ImGui::Separator();

            // Calibration grille (bouton bascule)
            {
                const bool wasGridEdit = g_gridEdit;
                if (wasGridEdit) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(110, 85, 15, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(150, 115, 25, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(90, 70, 10, 255));
                }
                if (ImGui::Button(wasGridEdit ? "(F8) Calibration : ON" : "(F8) Calibration grille",
                                  ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    g_gridEdit = !g_gridEdit;
                    if (g_gridEdit && g_clickThrough) SetClickThrough(false);
                }
                if (wasGridEdit) {
                    ImGui::PopStyleColor(3);
                    ImGui::TextDisabled("Fleches = origine   Molette = taille");
                    ImGui::TextDisabled("(%d, %d)  %dx%d",
                        grid.OriginX(), grid.OriginY(), grid.TileW(), grid.TileH());
                }
            }

            ImGui::Separator();
            if (ImGui::Button("(F10) Quitter", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
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
