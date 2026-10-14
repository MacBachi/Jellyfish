#pragma once
// Forced into the SDK's btstack_flash_bank.c only (see CMakeLists.txt). Running from an A/B
// partition, the address window at XIP_BASE starts at the partition, not at the start of the
// flash, so BTstack's two sectors near the end of the flash are not reachable there. This
// makes that file read them through the untranslated window instead, at the same offsets
// it writes them to.
#include "hardware/regs/addressmap.h"
#undef XIP_BASE
#define XIP_BASE XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
