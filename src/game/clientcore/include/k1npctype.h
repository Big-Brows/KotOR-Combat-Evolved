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
};
constexpr K1NpcType k1DarkJediTypes[] = {
    {"dark-jedi", "k1arena_dark", "k1darktest", "k1jadark", false, .82f, 3, 25, 85, 250000},
    {"dark-jedi-staff", "k1arena_darkstaff", "k1darkstaff", "k1jadstaff", true, .92f, 4, 20, 100, 150000}
};
