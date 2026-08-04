/* Compile-only ABI guard. Fails the build if the layout the Rust mirror in
 * phrame-common-rust/src/lib.rs assumes ever stops being true. */
#include <stdint.h>
#include <stddef.h>
#include "plugins.h"

/* C++ compatibility: _Static_assert is C11; use static_assert in C++ mode.
 * _Alignof is likewise C-only -- C++ spells it alignof -- so route both through
 * one macro rather than leaving the file C-only by accident. */
#ifdef __cplusplus
#define _Static_assert static_assert
#define PHRAME_ALIGNOF(t) alignof(t)
#else
#define PHRAME_ALIGNOF(t) _Alignof(t)
#endif

_Static_assert(sizeof(PluginFrameTrace) == 32,
               "PluginFrameTrace must be 32 bytes");
_Static_assert(sizeof(PluginFrame) == 1240,
               "PluginFrame must be 1240 bytes; bump PLUGIN_TRACE_SCHEMA / "
               "PLUGIN_INPUTS_SCHEMA and the Rust mirror together");
_Static_assert(offsetof(PluginFrame, trace_steps_used) == 520,
               "trace_steps_used moved");
_Static_assert(offsetof(PluginFrame, trace_schema) == 524,
               "trace_schema moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, trace_flags) == 526,
               "trace_flags moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, trace_steps) == 528,
               "trace_steps moved");

/* Contributing-input identities. Appended after trace_steps, so every offset
 * above is unchanged -- if one of them moves, the assertions above fire first
 * and say so, which is why these are listed after rather than instead. */
_Static_assert(sizeof(PluginFrameInput) == 24,
               "PluginFrameInput must be 24 bytes (u32 id + u32 reserved + 2x u64)");
_Static_assert(PHRAME_ALIGNOF(PluginFrameInput) == 8,
               "PluginFrameInput must be 8-byte aligned; the Rust mirror asserts the same");
_Static_assert(offsetof(PluginFrameInput, id) == 0, "PluginFrameInput.id moved");
_Static_assert(offsetof(PluginFrameInput, reserved) == 4,
               "PluginFrameInput.reserved moved; it exists to make this padding explicit");
_Static_assert(offsetof(PluginFrameInput, frame_index) == 8,
               "PluginFrameInput.frame_index moved");
_Static_assert(offsetof(PluginFrameInput, origination_time_ns) == 16,
               "PluginFrameInput.origination_time_ns moved");
_Static_assert(sizeof(((PluginFrame *)0)->inputs) == 192,
               "inputs[] must be MAX_PLUGIN_FRAME_INPUTS x 24 = 192 bytes");
_Static_assert(offsetof(PluginFrame, inputs_used) == 1040,
               "inputs_used moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, inputs_schema) == 1044,
               "inputs_schema moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, inputs_flags) == 1046,
               "inputs_flags moved; the Rust mirror reads this offset");
_Static_assert(offsetof(PluginFrame, inputs) == 1048,
               "inputs moved; the Rust mirror reads this offset");

_Static_assert(PHRAME_STAGE_OF(PHRAME_STAGE_PACK(pstComposite, 3)) == pstComposite,
               "stage_id pack/unpack must round-trip the stage");
_Static_assert(PHRAME_INSTANCE_OF(PHRAME_STAGE_PACK(pstComposite, 3)) == 3,
               "stage_id pack/unpack must round-trip the instance");

int main(void) { return 0; }

