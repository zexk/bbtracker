#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR ignored, int show) {
    (void)instance;
    (void)previous;
    (void)ignored;
    (void)show;

    wchar_t launcher[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, launcher, MAX_PATH);
    if (!length || length >= MAX_PATH)
        return 1;

    wchar_t *slash = wcsrchr(launcher, L'\\');
    if (!slash)
        return 1;
    *slash = L'\0';

    wchar_t game[MAX_PATH];
    wchar_t game_dir[MAX_PATH];
    wchar_t config[MAX_PATH];
    wchar_t stage[32];
    wchar_t command[2 * MAX_PATH];
    swprintf(game_dir, MAX_PATH, L"%ls\\..\\MGS4", launcher);
    swprintf(game, MAX_PATH, L"%ls\\mgs4.exe", game_dir);
    swprintf(config, MAX_PATH, L"%ls\\mgs4-stage-selector.ini", launcher);
    GetPrivateProfileStringW(L"fastLoad", L"stage", L"none", stage,
                             32, config);
    swprintf(command, 2 * MAX_PATH,
             L"\"%ls\" -region eu -lan en -selfregion EU -resolution 0 "
             L"-launcherpath launcher.exe -ctrltype AUTO -launcherroot \"%ls\"",
             game, launcher);
    if (wcscmp(stage, L"none") != 0) {
        size_t used = wcslen(command);
        swprintf(command + used, 2 * MAX_PATH - used, L" --stage %ls", stage);
    }

    STARTUPINFOW startup = { .cb = sizeof(startup) };
    PROCESS_INFORMATION process;
    if (!CreateProcessW(game, command, NULL, NULL, FALSE, 0, NULL, game_dir,
                        &startup, &process)) {
        wchar_t message[128];
        wsprintfW(message, L"Could not start mgs4.exe (error %lu).", GetLastError());
        MessageBoxW(NULL, message, L"MGS4 stage selector", MB_ICONERROR);
        return 1;
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    return (int)exit_code;
}
