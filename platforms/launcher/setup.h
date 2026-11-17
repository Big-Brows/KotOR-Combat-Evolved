// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
#pragma once
#include <filesystem>
#include <string>
std::string FileHash(const std::filesystem::path& path);
void PrepareArena(const std::filesystem::path& package, const std::filesystem::path& profile,
                  const std::filesystem::path& game, const std::filesystem::path& jka);
void CheckLocalPath(const std::filesystem::path& path);
