// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// KotOR:CE portable arena launcher, built on K12. No private settings are embedded.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "../macos/gamedirs.h"
#include "setup.h"
namespace fs = std::filesystem;
static bool quiet;
static int Fail(const wchar_t* message, int code) {
    if (!quiet) MessageBoxW(nullptr, message, L"KotOR:CE", MB_OK | MB_ICONERROR);
    return code;
}
static fs::path Select(const wchar_t* title) {
    BROWSEINFOW browse{}; browse.lpszTitle = title;
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    auto item = SHBrowseForFolderW(&browse);
    if (!item) return {};
    wchar_t path[MAX_PATH];
    bool valid = SHGetPathFromIDListW(item, path) != FALSE;
    CoTaskMemFree(item);
    return valid ? fs::path(path) : fs::path();
}
static bool Game(const fs::path& p) {
    return fs::is_regular_file(p / L"chitin.key") && fs::is_directory(p / L"data") &&
        fs::is_directory(p / L"modules") && fs::is_directory(p / L"streamwaves");
}
static bool Jka(const fs::path& p) {
    return fs::is_regular_file(p / L"base/assets0.pk3") && fs::is_regular_file(p / L"base/assets1.pk3");
}
static void Env(const wchar_t* key, const std::wstring& value) {
    if (!SetEnvironmentVariableW(key, value.c_str())) throw std::runtime_error("Environment failed");
}
static int Run() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool detect = false, smoke = false, prepare = false;
    fs::path game, jka;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--detect") == 0) detect = quiet = true;
        else if (wcscmp(argv[i], L"--smoke") == 0) smoke = quiet = true;
        else if (wcscmp(argv[i], L"--prepare-only") == 0) prepare = quiet = true;
        else if (wcscmp(argv[i], L"--game-dir") == 0 && i + 1 < argc) game = argv[++i];
        else if (wcscmp(argv[i], L"--jka-dir") == 0 && i + 1 < argc) jka = argv[++i];
        else { LocalFree(argv); return Fail(L"Supported: --detect, --smoke, --prepare-only, --game-dir PATH, --jka-dir PATH", 2); }
    }
    LocalFree(argv);
    wchar_t module[32768];
    DWORD length = GetModuleFileNameW(nullptr, module, 32768);
    if (!length || length == 32768) return Fail(L"Cannot locate the launcher.", 3);
    const auto package = fs::path(module).parent_path();
    auto environment = GetEnvironmentStringsW();
    std::vector<std::wstring> remove;
    for (auto p = environment; p && *p; p += wcslen(p) + 1)
        if (_wcsnicmp(p, L"K1_", 3) == 0 || _wcsnicmp(p, L"K2_", 3) == 0) {
            auto end = wcschr(p, L'='); if (end) remove.emplace_back(p, end);
        }
    FreeEnvironmentStringsW(environment);
    for (const auto& key : remove) { SetEnvironmentVariableW(key.c_str(), nullptr); _wputenv_s(key.c_str(), L""); }
    _putenv_s("K2_USER_DIR", "."); _putenv_s("K2_CLOUDSAVES", "0"); _putenv_s("K2_WORKSHOP", "0");
    if (game.empty()) {
        K2GameDirs dirs; std::string error;
        if (k2_find_game_dirs(true, dirs, error)) game = fs::path(dirs.assets);
        else if (!quiet) game = Select(L"Select your owned KotOR 1 installation (Steam or GOG)");
    }
    if (jka.empty()) {
        auto path = k2_find_jka_dir();
        if (!path.empty()) jka = fs::path(path);
        else if (!quiet) jka = Select(L"Select your owned Jedi Academy GameData folder");
    }
    if (!Game(game)) return Fail(L"KotOR 1 was not found. Install your owned Steam or GOG copy or select its folder.", 4);
    if (!Jka(jka)) return Fail(L"Owned Jedi Academy data is required. Select its GameData folder containing base/assets0.pk3 and assets1.pk3.", 5);
    if (detect) return 0;
    for (const auto& install : {game, jka}) {
        auto prefix = fs::absolute(install).lexically_normal().wstring() + L"\\";
        auto writable = (package / L"user-data").wstring();
        if (_wcsnicmp(prefix.c_str(), writable.c_str(), prefix.size()) == 0)
            return Fail(L"Extract the portable folder outside your game installations; they are read-only.", 7);
    }
    auto profile = package / L"user-data";
    CheckLocalPath(package); CheckLocalPath(profile);
    fs::create_directories(profile);
    HANDLE lock = CreateFileW((profile / L"launcher.lock").c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) return Fail(L"This portable copy is already running or its folder is not writable.", 7);
    struct Lock { HANDLE h; ~Lock() { CloseHandle(h); } } guard{lock};
    PrepareArena(package, profile, fs::absolute(game), fs::absolute(jka));
    if (prepare) return 0;
    const auto ini = profile / L"swkotor.ini";
    if (!fs::exists(ini)) {
        std::ofstream out(ini); out << "[Display Options]\nFullScreen=0\n[Graphics Options]\nEnableHardwareMouse=0\n";
    }
    auto saves = profile / L"saves"; CheckLocalPath(saves);
    if (!WritePrivateProfileStringW(L"Alias", nullptr, nullptr, ini.c_str()) ||
        !WritePrivateProfileStringW(L"Alias", L"SAVES", saves.c_str(), ini.c_str()))
        return Fail(L"Cannot set the portable save directory.", 7);
    if (!fs::exists(profile / L"k12.ini")) {
        std::ofstream out(profile / L"k12.ini"); out << "[Gameplay]\nFasterMovement=0\n";
    }
    Env(L"K2_ASSET_DIR", fs::absolute(game).wstring()); Env(L"K2_USER_DIR", profile.wstring());
    for (const auto* key : {L"K2_CLOUDSAVES", L"K2_WORKSHOP", L"K2_LOGFILE"}) Env(key, L"0");
    for (const auto* key : {L"K2_SKIP_MOVIES", L"K1_ENGAGE_PROTOTYPE", L"K1_ENGAGE_SANDBOX",
        L"K1_ENGAGE_STAFF_PROTOTYPE", L"K1_ENGAGE_DISMEMBERMENT", L"K1_ENGAGE_ANATOMICAL_RIG",
        L"K1_ENGAGE_VIEW_PITCH", L"K1_ENGAGE_ARENA", L"K1_ENGAGE_ENEMY_AI", L"K1_ENGAGE_DARK_JEDI"}) Env(key, L"1");
    Env(L"K1_ENGAGE_NPC_TYPE", L"dark-jedi"); Env(L"K1_WINDOW", L"1280x720");
    Env(L"K2_INPUT", smoke ? L"100:key:40;1200:quit:0" : L"100:key:40");
    if (smoke) Env(L"K2_HIDDEN", L"1");
    const auto exe = package / L"bin/k1.exe";
    std::wstring command = L"\"" + exe.wstring() + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    auto log = CreateFileW((profile / L"game.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) return Fail(L"Cannot open the portable game log.", 7);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = log;
    startup.hStdInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &security, OPEN_EXISTING, 0, nullptr);
    PROCESS_INFORMATION child{};
    bool started = CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, TRUE, 0, nullptr, profile.c_str(), &startup, &child) != FALSE;
    CloseHandle(log); if (startup.hStdInput != INVALID_HANDLE_VALUE) CloseHandle(startup.hStdInput);
    if (!started) return Fail(L"Could not start bin/k1.exe. Keep all portable package files together.", 8);
    CloseHandle(child.hThread);
    if (WaitForSingleObject(child.hProcess, smoke ? 120000 : INFINITE) == WAIT_TIMEOUT) {
        TerminateProcess(child.hProcess, 9); WaitForSingleObject(child.hProcess, 10000);
        CloseHandle(child.hProcess); return 9;
    }
    DWORD result = 1; GetExitCodeProcess(child.hProcess, &result); CloseHandle(child.hProcess);
    if (result) return Fail(L"The game exited with an error. See user-data/game.log.", 10);
    return 0;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int result;
    try { result = Run(); }
    catch (...) { result = Fail(L"Setup could not validate or prepare this data version. Check that both owned games and the complete portable package are present. Modified or different-language data may need a compatible package.", 11); }
    CoUninitialize();
    return result;
}
