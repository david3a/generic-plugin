/* Compile-only ABI guard. Fails the build if the layout the Rust mirror in
 * phrame-common-rust/src/lib.rs assumes ever stops being true. */
#include <stdint.h>
#include <stddef.h>
#include "plugins.h"

/* C++ compatibility: _Static_assert is C11; use static_assert in C++ mode. */
#ifdef __cplusplus
#define _Static_assert static_assert
#endif

_Static_assert(sizeof(PluginFrameTrace) == 32,
               "PluginFrameTrace must be 32 bytes");
_Static_assert(sizeof(PluginFrame) == 1040,
               "PluginFrame must be 1040 bytes; bump PLUGIN_TRACE_SCHEMA and the Rust mirror together");
_Static_assert(offsetof(PluginFrame, trace_steps_used) == 520,
               "trace_steps_used moved");
_Static_assert(offsetof(PluginFrame, trace_schema) == 524,
               "trace_schema moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, trace_flags) == 526,
               "trace_flags moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, trace_steps) == 528,
               "trace_steps moved");
_Static_assert(PHRAME_STAGE_OF(PHRAME_STAGE_PACK(pstComposite, 3)) == pstComposite,
               "stage_id pack/unpack must round-trip the stage");
_Static_assert(PHRAME_INSTANCE_OF(PHRAME_STAGE_PACK(pstComposite, 3)) == 3,
               "stage_id pack/unpack must round-trip the instance");

int main(void) { return 0; }
