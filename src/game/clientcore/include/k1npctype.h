#pragma once

// Stable local spawn identities. A future sandbox selector can enumerate these
// without conflating body appearance with fighting style.
struct K1NpcType {
    const char* id;
    const char* tag;
    const char* creature;
    const char* rig;
    bool staff;
    float swingRate;
    int chainLimit;
    int parryPercent;
    int chainContinuePercent;
    long long attackRestUs;
    bool dual=false;
    bool red=false;
    bool blue=false;
};
constexpr K1NpcType k1DarkJediTypes[] = {
    {"dark-jedi", "k1arena_dark", "k1darktest", "k1jadark", false, .85f, 3, 25, 85, 250000},
    {"dark-jedi-staff", "k1arena_darkstaff", "k1darkstaff", "k1jadstaff", true, .85f, 4, 20, 100, 150000},
    {"dark-jedi-dual", "k1arena_darkdual", "k1darkdual", "k1jaddual", false, .75f, 3, 25, 85, 250000, true},
    {"dark-jedi-red", "k1arena_darkred", "k1darkred", "k1jadred", false, 1.f, 3, 25, 85, 250000, false, true}
    ,{"dark-jedi-blue", "k1arena_darkblue", "k1darkblue", "k1jadblue", false, 1.f, 4, 25, 85, 250000, false, false, true}
};
