/* Compile-time proof that a conforming implementation matches query_stats_fn.
 * Mirrors tests/abi_layout_check.c: this test fails by not compiling if the
 * typedef is missing or its signature changes underneath callers.
 *
 * It also carries one runtime check on the too-small-buffer path: unlike
 * query_config_fn, query_stats_fn must not truncate. A truncated JSON
 * payload is invalid JSON, so the contract is prcSmallBuffer and nothing
 * written, not a partial copy. */
#include "plugins.h"
#include <string.h>

static int32_t sample_query_stats(PLUGIN_HANDLE handle, char *buffer, uint32_t buffer_size)
{
    const char *json = "{\"packets\":0}";
    const uint32_t need = (uint32_t)strlen(json) + 1u;
    (void)handle;
    if (buffer == NULL || buffer_size < need) {
        return (int32_t)prcSmallBuffer;  /* too small: write nothing, say so */
    }
    memcpy(buffer, json, need);
    return (int32_t)(need - 1u);  /* success: bytes written, excluding NUL */
}

int main(void)
{
    query_stats_fn fn = sample_query_stats;
    char buf[64];
    char tiny[1];

    /* Passing case: buffer is big enough, returns the byte count. */
    int32_t written = fn(0, buf, (uint32_t)sizeof(buf));
    if (written != (int32_t)strlen("{\"packets\":0}")) {
        return 1;
    }

    /* Too-small case: returns prcSmallBuffer, not a positive size hint. */
    int32_t result = fn(0, tiny, (uint32_t)sizeof(tiny));
    if (result != (int32_t)prcSmallBuffer) {
        return 1;
    }

    return 0;
}
