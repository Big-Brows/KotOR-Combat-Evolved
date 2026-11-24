// Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
// Component/integration probe, separate from the shipping executable.
#include "lazy_model.h"
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>
#include <psapi.h>
namespace fs = std::filesystem;
static bool Check(const fs::path& path, const fs::path& expected = {}) {
    auto name = path.u8string(); FILE* f = k1_lazy_model_open(_wfopen(path.c_str(), L"rb"), name.c_str());
    if (!f) return false;
    _fseeki64(f, 0, SEEK_END); auto size = _ftelli64(f);
    bool ok = size >= 0;
    FILE* reference = expected.empty() ? nullptr : _wfopen(expected.c_str(), L"rb");
    if (!expected.empty() && !reference) ok = false;
    if (reference) { _fseeki64(reference,0,SEEK_END); ok &= _ftelli64(reference) == size; }
    std::vector<unsigned char> a(65536), b(65536);
    // Nonsequential seeks cross chunk boundaries and verify size/EOF semantics.
    for (long long offset : {0ll, size/2, size > 65536 ? size-65536 : 0ll, size}) {
        if (_fseeki64(f,offset,SEEK_SET)) ok = false;
        auto n = fread(a.data(),1,a.size(),f);
        ok &= n == (size_t)((size-offset) < (long long)a.size() ? size-offset : a.size());
        if (reference) {
            _fseeki64(reference,offset,SEEK_SET);
            ok &= fread(b.data(),1,b.size(),reference) == n && !memcmp(a.data(),b.data(),n);
        }
    }
    if (reference) {
        rewind(f); rewind(reference);
        for (;;) {
            auto n = fread(a.data(),1,a.size(),f);
            ok &= fread(b.data(),1,b.size(),reference) == n && !memcmp(a.data(),b.data(),n);
            if (!n) break;
        }
        fclose(reference);
    }
    fclose(f); return ok;
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 3 && !wcscmp(argv[1],L"--stress")) {
        for(int i=0;i<20;++i) if(!Check(argv[2]))return 1;
        // Hold two independent streams across an additional reconstruction. Their
        // backing files must survive until their respective native owner closes.
        auto path=fs::path(argv[2]);auto name=path.u8string();
        FILE* a=k1_lazy_model_open(_wfopen(path.c_str(),L"rb"),name.c_str());
        FILE* b=k1_lazy_model_open(_wfopen(path.c_str(),L"rb"),name.c_str());
        if(!a || !b)return 1;
        unsigned char x[1024],y[1024];auto n=fread(x,1,sizeof x,a);fclose(a);
        bool ok=fread(y,1,sizeof y,b)==n && !memcmp(x,y,n);fclose(b);
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof memory;
        if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&memory,sizeof memory))return 1;
        printf("LAZY_MODEL_STRESS_%s loads=22 peak_working_set=%zu private_bytes=%zu\n",
            ok?"PASS":"FAIL",(size_t)memory.PeakWorkingSetSize,(size_t)memory.PrivateUsage);
        return ok?0:1;
    }
    if (argc == 3 && !wcscmp(argv[1],L"--all")) {
        unsigned count=0;
        for (const auto& entry : fs::directory_iterator(argv[2])) {
            auto ext=entry.path().extension();
            if (ext == L".mdl" || ext == L".mdx") {
                if (!Check(entry.path())) return 1;
                ++count;
            }
        }
        printf("LAZY_MODEL_ALL_PASS count=%u\n",count); return count ? 0 : 1;
    }
    if (argc < 2 || argc > 3) return 2;
    bool ok = Check(argv[1], argc == 3 ? fs::path(argv[2]) : fs::path());
    puts(ok ? "LAZY_MODEL_CHECK_PASS" : "LAZY_MODEL_CHECK_FAIL"); return ok ? 0 : 1;
}
