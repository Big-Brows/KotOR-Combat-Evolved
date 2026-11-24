#pragma once

// Eligibility only: contacts are never held until the window expires.
// World time respects hard pause and planning speed. Each actual swing/chain
// start opens a new window; no facing, direction, input-edge or use-count gate.
namespace K1AttackDeflection {
constexpr long long windowUs=500000;
inline bool Eligible(long long startedAt,long long now) {
    return startedAt>=0 && now>=startedAt && now-startedAt<=windowUs;
}
}
