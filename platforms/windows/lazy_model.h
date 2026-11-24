// Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
// Lossless portable MDL/MDX storage. Normal files pass through unchanged.
#pragma once
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
// Takes ownership of input, including on failure. path is the resolved host path.
FILE* k1_lazy_model_open(FILE* input, const char* path);
#ifdef __cplusplus
}
#endif
