#include <windows.h>
#include <wchar.h>

enum { ID_STAGE = 100, ID_RESTART = 101 };
typedef struct { const wchar_t *code; const wchar_t *name; } Stage;
static const Stage stages[] = {
    {L"s00a00l", L"Prologue Cemetery"}, {L"s00a10l", L"Ending Cemetery"},
    {L"s01a00l", L"Middle East Infiltration"}, {L"s01a05l", L"Middle East Infiltration"},
    {L"s01a10l", L"Red Zone"}, {L"s01a20l", L"Militia Safehouse"},
    {L"s01a30l", L"Urban Ruins"}, {L"s01a40l", L"Advent Palace"},
    {L"s01a50l", L"Crescent Meridian"}, {L"s01a55l", L"Crescent Meridian"},
    {L"s01a57l", L"Millennium Park"}, {L"s01a60l", L"Liquid's Encampment"},
    {L"s02a10l", L"Cove Valley Village"}, {L"s02a20l", L"Power Station"},
    {L"s02a25l", L"Power Station"}, {L"s02a30l", L"Confinement Facility"},
    {L"s02a40l", L"Vista Mansion"}, {L"s02a50l", L"Research Lab"},
    {L"s02a60l", L"Mountain Trail / Riverside"}, {L"s02a70l", L"Vamp Ambush"},
    {L"s02a73l", L"Stryker Escape"}, {L"s02a75l", L"Stryker Escape"},
    {L"s02a78l", L"Stryker Escape"}, {L"s02a80l", L"High Woodlands Highway"},
    {L"s02a85l", L"Marketplace Entrance"}, {L"s02a90l", L"Marketplace"},
    {L"s02a95l", L"Marketplace Plaza"}, {L"s03a00l", L"Eastern Europe Station"},
    {L"s03a10l", L"Midtown: Resistance Tail"}, {L"s03a15l", L"Midtown: Resistance Tail"},
    {L"s03a16l", L"Midtown: Canals"}, {L"s03a20l", L"Midtown: Plaza"},
    {L"s03a25l", L"Midtown: North Sector"}, {L"s03a30l", L"Church Courtyard"},
    {L"s03a35l", L"Motorcycle Chase"}, {L"s03a40l", L"Motorcycle Chase"},
    {L"s03a50l", L"Raging Raven Ambush"}, {L"s03a60l", L"Motorcycle Chase"},
    {L"s03a65l", L"Echo's Beacon"}, {L"s03a70l", L"Echo's Beacon"},
    {L"s03a90l", L"Volta River"}, {L"s04a05l", L"Metal Gear Solid Flashback"},
    {L"s04a10l", L"Snowfield / Heliport / Tank Hangar"},
    {L"s04a20l", L"Nuclear Warhead Storage Building"},
    {L"s04a30l", L"Snowfield / Communications Tower"},
    {L"s04a40l", L"Blast Furnace / Casting Facility"},
    {L"s04a50l", L"Underground Base"}, {L"s04a60l", L"Underground Supply Tunnel"},
    {L"s04a65l", L"REX Escape"}, {L"s04a68l", L"Port Area"},
    {L"s04a70l", L"Port Area: REX vs. RAY"}, {L"s04a75l", L"Outer Haven Arrival"},
    {L"s05a10l", L"Ship Bow"}, {L"s05a20l", L"Command Center / Missile Hangar"},
    {L"s05a30l", L"Microwave Corridor"}, {L"s05a40l", L"GW"},
    {L"s05a45l", L"Liquid Ocelot: Prelude"}, {L"s05a50l", L"Liquid Ocelot"},
    {L"s05a55l", L"Liquid Ocelot: Aftermath"},
    {L"s10a10l", L"Nomad Mission Briefing"},
    {L"s10a20l", L"Nomad: South America Briefing"},
    {L"s10a30l", L"Nomad: Eastern Europe Briefing"},
    {L"s10a40l", L"Nomad: Shadow Moses Briefing"},
    {L"s20a00l", L"USS Missouri"}, {L"s20a10l", L"USS Missouri vs. Outer Haven"},
    {L"s20a20l", L"Campbell's Room"}, {L"s30a00l", L"Wedding"},
    {L"s30a10l", L"Hospital"},
};

static wchar_t launcher_dir[MAX_PATH];
static HWND combo;
static HWND game_window;

static BOOL CALLBACK find_game_window(HWND window, LPARAM unused)
{
    (void)unused;
    DWORD process = 0;
    wchar_t class_name[32];
    GetClassNameW(window, class_name, 32);
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId() && IsWindowVisible(window)
        && GetWindowTextLengthW(window) > 0
        && wcscmp(class_name, L"MGS4StageSelector") != 0) {
        game_window = window;
        return FALSE;
    }
    return TRUE;
}

static void align_to_game(HWND window)
{
    game_window = NULL;
    EnumWindows(find_game_window, 0);
    if (!game_window) return;
    RECT game, selector;
    GetWindowRect(game_window, &game);
    GetWindowRect(window, &selector);
    const int width = selector.right - selector.left;
    const int height = selector.bottom - selector.top;
    SetWindowLongPtrW(window, GWLP_HWNDPARENT, (LONG_PTR)game_window);
    SetWindowPos(window, HWND_TOP,
                 game.left + (game.right - game.left - width) / 2,
                 game.top + (game.bottom - game.top - height) / 2,
                 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

static void restart_into_stage(HWND window)
{
    int selected = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (selected < 0 || selected >= (int)(sizeof(stages) / sizeof(stages[0]))) return;
    wchar_t config[MAX_PATH], launcher[MAX_PATH], command[MAX_PATH + 4];
    swprintf(config, MAX_PATH, L"%ls\\mgs4-stage-selector.ini", launcher_dir);
    if (!WritePrivateProfileStringW(L"fastLoad", L"stage", stages[selected].code, config)) {
        MessageBoxW(window, L"Could not write stage config.", L"MGS4 stage selector", MB_ICONERROR);
        return;
    }
    swprintf(launcher, MAX_PATH, L"%ls\\launcher.exe", launcher_dir);
    swprintf(command, MAX_PATH + 4, L"\"%ls\"", launcher);
    STARTUPINFOW startup = { .cb = sizeof(startup) };
    PROCESS_INFORMATION process;
    if (!CreateProcessW(launcher, command, NULL, NULL, FALSE, 0, NULL, launcher_dir,
                        &startup, &process)) {
        MessageBoxW(window, L"Could not restart game.", L"MGS4 stage selector", MB_ICONERROR);
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    ExitProcess(0);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    if (message == WM_COMMAND && LOWORD(wparam) == ID_RESTART) {
        restart_into_stage(window);
        return 0;
    }
    if (message == WM_CLOSE) {
        ShowWindow(window, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static DWORD WINAPI selector_thread(void *unused)
{
    (void)unused;
    wchar_t game[MAX_PATH];
    GetModuleFileNameW(NULL, game, MAX_PATH);
    wchar_t *slash = wcsrchr(game, L'\\');
    if (!slash) return 1;
    *slash = L'\0';
    swprintf(launcher_dir, MAX_PATH, L"%ls\\..\\Launcher", game);
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    cls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    cls.lpszClassName = L"MGS4StageSelector";
    RegisterClassW(&cls);
    HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls.lpszClassName,
                                  L"MGS4 Stage Selector",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 510, 135,
                                  NULL, NULL, instance, NULL);
    combo = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                          12, 14, 470, 400, window, (HMENU)ID_STAGE, instance, NULL);
    HWND restart = CreateWindowW(L"BUTTON", L"Restart into stage",
                                 WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 330, 52, 152, 30, window,
                                 (HMENU)ID_RESTART, instance, NULL);
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessageW(combo, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(restart, WM_SETFONT, (WPARAM)font, TRUE);
    wchar_t current[32], config[MAX_PATH], label[160];
    swprintf(config, MAX_PATH, L"%ls\\mgs4-stage-selector.ini", launcher_dir);
    GetPrivateProfileStringW(L"fastLoad", L"stage", L"s01a20l", current, 32, config);
    int current_index = 0;
    for (int i = 0; i < (int)(sizeof(stages) / sizeof(stages[0])); ++i) {
        swprintf(label, 160, L"%ls  -  %ls", stages[i].code, stages[i].name);
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label);
        if (wcscmp(current, stages[i].code) == 0) current_index = i;
    }
    SendMessageW(combo, CB_SETCURSEL, current_index, 0);
    RegisterHotKey(NULL, 1, 0, VK_F6);
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        if (message.message == WM_HOTKEY) {
            align_to_game(window);
            SetForegroundWindow(window);
        } else {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(NULL, 0, selector_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
