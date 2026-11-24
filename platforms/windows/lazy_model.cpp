// Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
// Kept outside the engine's no-exception/Win32 compatibility dialect.
#include "lazy_model.h"
#include <windows.h>
#include <bcrypt.h>
#include <msdelta.h>
#include <io.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <cstring>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
constexpr size_t InputLimit = 64 * 1024 * 1024;
constexpr size_t ChunkLimit = 16 * 1024 * 1024;
std::mutex restoreMutex;
void Check(bool ok) { if (!ok) throw std::runtime_error("Invalid lazy model"); }
bool Hex(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
struct Hasher {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    Hasher() {
        Check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0);
        if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(algorithm, 0); algorithm = nullptr; Check(false);
        }
    }
    ~Hasher() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    void Add(const void* p, size_t size) {
        Check(size <= 0xffffffffull && BCryptHashData(hash, (PUCHAR)p, (ULONG)size, 0) == 0);
    }
    std::string End() {
        unsigned char digest[32]; Check(BCryptFinishHash(hash, digest, sizeof digest, 0) == 0);
        std::string out; const char* hex = "0123456789abcdef";
        for (auto c : digest) { out += hex[c >> 4]; out += hex[c & 15]; }
        return out;
    }
};
void Local(const fs::path& path) {
    for (auto p = fs::absolute(path); !p.empty();) {
        DWORD a = GetFileAttributesW(p.c_str());
        Check(a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_REPARSE_POINT));
        auto parent = p.parent_path(); if (parent == p) break; p = parent;
    }
}
Bytes Read(const fs::path& path, const std::string& hash) {
    Local(path);
    std::ifstream f(path, std::ios::binary | std::ios::ate); Check(bool(f));
    auto n = f.tellg(); Check(n >= 0 && n <= InputLimit);
    Bytes b(static_cast<size_t>(n)); f.seekg(0); f.read((char*)b.data(), b.size()); Check(bool(f));
    Hasher h; h.Add(b.data(), b.size()); Check(h.End() == hash); return b;
}
using File = std::unique_ptr<FILE, decltype(&fclose)>;
File Temporary() {
    wchar_t folder[MAX_PATH], name[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, folder); Check(n && n < MAX_PATH);
    Check(GetTempFileNameW(folder, L"k1m", 0, name) != 0);
    HANDLE h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) { DeleteFileW(name); Check(false); }
    int fd = _open_osfhandle((intptr_t)h, _O_BINARY | _O_RDWR);
    if (fd < 0) { CloseHandle(h); Check(false); }
    FILE* f = _fdopen(fd, "w+b");
    if (!f) { _close(fd); Check(false); }
    return File(f, &fclose);
}
File Restore(FILE* descriptor, const fs::path& path) {
    // All descriptor fields are fixed-length lowercase hashes, never arbitrary paths.
    char text[196]; Check(fread(text, 1, sizeof text, descriptor) == 195 && feof(descriptor));
    std::string s(text, 195);
    Check(s[64] == '\n' && s[129] == '\n' && s[194] == '\n');
    std::string baseHash = s.substr(0,64), patchHash = s.substr(65,64), targetHash = s.substr(130,64);
    Check(Hex(baseHash) && Hex(patchHash) && Hex(targetHash));
    Local(path);
    auto store = path.parent_path() / L".k1models";
    auto base = Read(store / (baseHash + ".base"), baseHash);
    auto patch = Read(store / (patchHash + ".delta"), patchHash);
    Check(patch.size() >= 8 && !memcmp(patch.data(), "KDP1", 4));
    auto number = [&](size_t at) { unsigned n; Check(at <= patch.size()-4); memcpy(&n, patch.data()+at,4); return n; };
    unsigned count = number(4); Check(count <= 32); // Empty native MDX files are valid.
    size_t at = 8, total = 0;
    auto file = Temporary(); Hasher hash;
    DELTA_INPUT source{}; source.lpStart = base.data(); source.uSize = base.size();
    for (unsigned i=0; i<count; ++i) {
        unsigned size = number(at); at += 4; Check(size && size <= patch.size()-at);
        DELTA_INPUT delta{}; delta.lpStart = patch.data()+at; delta.uSize = size;
        // Inspect target size before asking MSDelta to allocate it.
        DELTA_HEADER_INFO header{}; Check(GetDeltaInfoB(delta, &header) != FALSE);
        Check(header.TargetSize <= ChunkLimit);
        DELTA_OUTPUT out{}; Check(ApplyDeltaB(DELTA_FLAG_NONE, source, delta, &out) != FALSE);
        struct Free { void operator()(void* p) const { if(p) DeltaFree(p); } };
        std::unique_ptr<void,Free> owned(out.lpStart);
        Check(out.uSize == header.TargetSize && out.uSize <= ChunkLimit);
        hash.Add(out.lpStart, out.uSize);
        Check(fwrite(out.lpStart, 1, out.uSize, file.get()) == out.uSize);
        total += out.uSize; at += size;
    }
    Check(at == patch.size() && hash.End() == targetHash);
    Check(fflush(file.get()) == 0 && _fseeki64(file.get(), 0, SEEK_SET) == 0);
    fprintf(stderr, "K1_LAZY_MODEL_PASS bytes=%zu chunks=%u\n", total, count);
    return file;
}
}
extern "C" FILE* k1_lazy_model_open(FILE* input, const char* path) {
    if (!input) return nullptr;
    char magic[5]; size_t got = fread(magic, 1, sizeof magic, input);
    if (got != 5 || memcmp(magic, "KLM1\n", 5)) { rewind(input); return input; }
    File descriptor(input, &fclose);
    try {
        // No expanded RAM cache. K12 retains parsed/resident resources itself. Only one
        // base (<=64MiB), patch (<=64MiB) and target chunk (<=16MiB) at a time here.
        // MSDelta has additional internal scratch allocations. Streams live on disk
        // until their native resource owner closes them; never evict an active stream.
        std::lock_guard<std::mutex> lock(restoreMutex);
        return Restore(input, fs::u8path(path)).release();
    } catch (...) {
        fprintf(stderr, "K1_LAZY_MODEL_FAIL validation_or_io\n");
        return nullptr;
    }
}
