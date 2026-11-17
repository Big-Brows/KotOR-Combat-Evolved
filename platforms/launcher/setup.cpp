// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
// Versioned owned-data patches. Source installations are only ever opened for read.
#include <windows.h>
#include <bcrypt.h>
#include <msdelta.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include "setup.h"
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
static void Require(bool ok) { if (!ok) throw std::runtime_error("Portable data validation failed"); }
static Bytes Read(const fs::path& path, unsigned long long offset = 0,
                  unsigned long long size = ~0ull) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Require(bool(input));
    auto total = static_cast<unsigned long long>(input.tellg());
    Require(offset <= total);
    if (size == ~0ull) size = total - offset;
    Require(size <= total - offset && size <= 512ull * 1024 * 1024);
    Bytes out(static_cast<size_t>(size));
    input.seekg(offset);
    input.read(reinterpret_cast<char*>(out.data()), out.size());
    Require(bool(input));
    return out;
}
static std::string Hash(const void* data, size_t size) {
    BCRYPT_ALG_HANDLE algorithm{};
    Require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0);
    unsigned char digest[32];
    auto status = BCryptHash(algorithm, nullptr, 0, (PUCHAR)data, static_cast<ULONG>(size), digest, 32);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    Require(status == 0);
    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (auto b : digest) { out += hex[b >> 4]; out += hex[b & 15]; }
    return out;
}
std::string FileHash(const fs::path& path) { auto bytes = Read(path); return Hash(bytes.data(), bytes.size()); }
void CheckLocalPath(const fs::path& path) {
    for (auto p = fs::absolute(path); !p.empty(); ) {
        DWORD attributes = GetFileAttributesW(p.c_str());
        Require(attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_REPARSE_POINT));
        auto parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
}
static fs::path Relative(const std::string& text) {
    fs::path path = fs::u8path(text);
    Require(!path.empty() && !path.is_absolute() && !path.has_root_name());
    Require(text.find(':') == std::string::npos && text.find('\\') == std::string::npos);
    for (const auto& part : path) Require(part != L".." && part != L".");
    return path;
}
void PrepareArena(const fs::path& package, const fs::path& profile,
                  const fs::path& game, const fs::path& jka) {
    // The two versioned tables are deliberately plain relative names and hashes.
    // No timestamps, local build paths, configuration or log files are included.
    std::ifstream sources(package / L"data/sources.tsv");
    Require(bool(sources));
    std::vector<Bytes> bases;
    std::string line;
    while (std::getline(sources, line)) {
        std::istringstream row(line);
        std::string kind, name, offset, size, hash;
        Require(bool(std::getline(row, kind, '\t') && std::getline(row, name, '\t') &&
            std::getline(row, offset, '\t') && std::getline(row, size, '\t') && std::getline(row, hash)));
        Require(kind == "K" || kind == "J");
        auto bytes = Read((kind == "K" ? game : jka) / Relative(name), std::stoull(offset), std::stoull(size));
        Require(Hash(bytes.data(), bytes.size()) == hash);
        bases.push_back(std::move(bytes));
    }
    Require(!bases.empty());
    std::ifstream manifest(package / L"data/patches.tsv");
    Require(bool(manifest));
    const auto seed = profile / L"saves/000003 - Game2";
    const auto pendingSeed = profile / L"seed-pending";
    bool needsSeed = !fs::exists(seed);
    while (std::getline(manifest, line)) {
        std::istringstream row(line);
        std::string name, index, patchName, hash;
        Require(bool(std::getline(row, name, '\t') && std::getline(row, index, '\t') &&
            std::getline(row, patchName, '\t') && std::getline(row, hash)));
        auto destination = profile / Relative(name);
        if (name.compare(0, 6, "saves/") == 0) {
            if (!needsSeed) continue;
            destination = pendingSeed / Relative(name).filename();
        }
        CheckLocalPath(destination);
        // First-run fixture saves are never written over a player's saves.
        if (fs::exists(destination) && FileHash(destination) == hash) continue;
        auto sourceIndex = std::stoull(index);
        Require(sourceIndex < bases.size());
        auto patch = Read(package / L"data" / Relative(patchName));
        auto& base = bases[sourceIndex];
        DELTA_INPUT source{};
        source.lpStart = base.data(); source.uSize = base.size();
        Require(patch.size() >= 8 && memcmp(patch.data(), "KDP1", 4) == 0);
        auto number = [&](size_t at) { unsigned value; Require(at <= patch.size() - 4); memcpy(&value, patch.data() + at, 4); return value; };
        unsigned count = number(4); Require(count <= 32);
        size_t position = 8;
        Bytes restored;
        for (unsigned part = 0; part < count; ++part) {
            unsigned size = number(position); position += 4;
            Require(size <= patch.size() - position);
            DELTA_INPUT delta{}; delta.lpStart = patch.data() + position; delta.uSize = size;
            DELTA_OUTPUT output{};
            Require(ApplyDeltaB(DELTA_FLAG_NONE, source, delta, &output) != FALSE);
            Require(output.uSize <= 16 * 1024 * 1024);
            auto begin = static_cast<unsigned char*>(output.lpStart);
            restored.insert(restored.end(), begin, begin + output.uSize);
            DeltaFree(output.lpStart); position += size;
        }
        Require(position == patch.size() && Hash(restored.data(), restored.size()) == hash);
        fs::create_directories(destination.parent_path());
        auto pending = destination; pending += L".pending";
        CheckLocalPath(pending);
        std::ofstream file(pending, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(restored.data()), restored.size());
        file.close(); Require(bool(file));
        Require(MoveFileExW(pending.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE);
    }
    if (needsSeed) {
        CheckLocalPath(seed); CheckLocalPath(pendingSeed);
        fs::create_directories(seed.parent_path());
        Require(MoveFileExW(pendingSeed.c_str(), seed.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE);
    }
}
