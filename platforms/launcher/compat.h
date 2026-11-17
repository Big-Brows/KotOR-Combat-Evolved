// Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
#pragma once
#include <sys/stat.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
