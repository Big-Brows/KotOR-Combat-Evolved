// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// Synthetic store layouts. Registry reads are replaced inside this test TU;
// no real registry or game installation is changed.
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <map>
#include "../macos/gamedirs.h"
namespace fs = std::filesystem;
static std::map<std::string, std::string> registry;
static LSTATUS FakeRegGetValue(HKEY, LPCSTR key, LPCSTR value, DWORD, LPDWORD, PVOID data, LPDWORD size) {
    auto found = registry.find(std::string(key) + "/" + value);
    if (found == registry.end()) return ERROR_FILE_NOT_FOUND;
    auto length = found->second.size() + 1;
    if (*size < length) return ERROR_MORE_DATA;
    memcpy(data, found->second.c_str(), length); *size = static_cast<DWORD>(length);
    return ERROR_SUCCESS;
}
#define RegGetValueA FakeRegGetValue
#include "../macos/gamedirs.cpp"
#undef RegGetValueA
static void Check(bool ok) { if (!ok) throw std::runtime_error("discovery check failed"); }
static void Touch(const fs::path& p) { fs::create_directories(p.parent_path()); std::ofstream(p).put('\0'); }
static void Value(const char* key, const char* value, const fs::path& path) {
    registry[std::string(key) + "/" + value] = path.string();
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    fs::path root = fs::absolute(argv[1]); fs::create_directories(root);
    SetCurrentDirectoryW(root.c_str());
    for (const auto* v : {"K2_ASSET_DIR", "K2_DATA_DIR", "K2_OBB_DIR", "K1_OBB_DIR", "K2_OBB"}) _putenv_s(v, "");
    for (const auto* v : {"ProgramFiles", "ProgramFiles(x86)", "USERPROFILE"}) _putenv_s(v, root.string().c_str());
    _putenv_s("K2_USER_DIR", root.string().c_str()); _putenv_s("K2_CLOUDSAVES", "0"); _putenv_s("K2_WORKSHOP", "0");
    int result = 0;
    const char* stage = "gog";
    try {
        auto kotor = root / "gog-k1";
        Touch(kotor / "chitin.key"); fs::create_directories(kotor / "streamwaves");
        Value("SOFTWARE\\GOG.com\\Games\\1207666283", "path", kotor);
        K2GameDirs dirs; std::string error;
        Check(k2_find_game_dirs(true, dirs, error) && fs::equivalent(dirs.assets, kotor));
        stage = "reject-k2";
        auto wrong = root / "gog-k2";
        Touch(wrong / "chitin.key"); fs::create_directories(wrong / "streamvoice");
        Value("SOFTWARE\\GOG.com\\Games\\1207666283", "path", wrong);
        Check(!k2_find_game_dirs(true, dirs, error));
        stage = "secondary-steam";
        auto steam = root / "Steam"; auto library = root / "Other Library";
        fs::create_directories(steam / "steamapps");
        std::ofstream(steam / "steamapps/libraryfolders.vdf") << "\"libraryfolders\" { \"1\" { \"path\" \"" << library.generic_string() << "\" } }\n";
        auto installed = library / "steamapps/common/swkotor";
        Touch(installed / "chitin.key"); fs::create_directories(installed / "streamwaves");
        Check(k2_find_game_dirs(true, dirs, error) && fs::equivalent(dirs.assets, installed));
        stage = "jka";
        auto jka = library / "steamapps/common/Jedi Academy/GameData";
        Touch(jka / "base/assets0.pk3"); Touch(jka / "base/assets1.pk3");
        Check(fs::equivalent(k2_find_jka_dir(), jka));
        std::cout << "DISCOVERY_PASS gog_k1=1 reject_k2=1 secondary_steam=1 jka=1\n";
    } catch (...) { std::cout << "DISCOVERY_FAIL stage=" << stage << "\n"; result = 1; }
    return result;
}
