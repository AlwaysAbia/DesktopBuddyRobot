// Single place that pulls in include/secrets.h (gitignored), falling back to
// the empty example so the project still builds on a fresh clone.
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found - building with empty credentials (OFFLINE mode)"
#include "secrets.example.h"
#endif

// secrets.h files created before ThingsBoard support don't define this.
#ifndef TB_ACCESS_TOKEN
#warning "TB_ACCESS_TOKEN missing from include/secrets.h - ThingsBoard disabled (see secrets.example.h)"
#define TB_ACCESS_TOKEN ""
#endif
