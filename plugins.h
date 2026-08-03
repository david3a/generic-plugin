#ifndef PLUGINS_H_
#define PLUGINS_H_

#define PLUGINS_VERSION_MAJOR 0
#define PLUGINS_VERSION_MINOR 0
#define PLUGINS_VERSION_PATCH 2

// Combine version numbers into single value for easier comparison
#define PLUGINS_VERSION                                                        \
    ((PLUGINS_VERSION_MAJOR * 10000) + (PLUGINS_VERSION_MINOR * 100)           \
     + (PLUGINS_VERSION_PATCH))

#include <stdint.h>
#include <uuid/uuid.h>

typedef struct PluginRational
{
    uint64_t Numerator;
    uint64_t Denominator;
} PluginRational;

typedef enum
{
    cfRGB,
    cfYUV
} ColourFormat;

typedef enum
{
    pdtNone = 0,
    pdtVideo,
    pdtAudio,
    pdtAncillary
} PluginDataType;

typedef struct plugin_uuid
{
unsigned char uuid[16];
} plugin_uuid;

#define MAX_PLUGIN_PLANES (4)

typedef struct PluginComponent
{
    // pointer to each plane present
    void *data;

    // size in bytes of each element,
    uint32_t stride;

    // width in elements
    uint32_t width;

    // size of one element (pixel, audio sample)
    uint32_t bytes_per_element;

    // number of bits in each component
    uint32_t bit_depth;

} PluginComponent;

#define FRAME_TYPE_MASK (0xf)
#define FRAME_TYPE_I_FRAME (0x1)
#define FRAME_TYPE_B_FRAME (0x2)
#define FRAME_TYPE_P_FRAME (0x3)
#define FRAME_TYPE_KEY_FRAME (0x10)

/* ── Frame stage trace ──────────────────────────────────────────────────
 * Each processing step appends one entry, so a frame's whole lifetime can be
 * reconstructed wherever the chain ends:
 *
 *   leave_ns - enter_ns              wall time inside the stage
 *   next.enter_ns - leave_ns         dwell BETWEEN stages (e.g. sat in a VDI ring)
 *   (leave_ns - enter_ns) - processing_ns
 *                                    time the stage spent waiting rather than
 *                                    working: lock contention, pacing, spinning
 *
 * Both timestamps are required. Deriving a stage's duration from the NEXT
 * stage's enter_ns would silently fold the inter-stage dwell into it, and that
 * dwell is the whole point.
 */
#define MAX_PLUGIN_TRACE_STEPS (16)
#define PLUGIN_TRACE_SCHEMA (1)
#define PLUGIN_TRACE_FLAG_TRUNCATED (0x1u)

/* stage_id packs the stage in the low 32 bits and the instance in the high 32,
 * so an entry says WHICH compositor or player produced it without costing extra
 * bytes. Use the macros; do not hand-roll the shifts. */
#define PHRAME_STAGE_PACK(stage, instance) \
    (((uint64_t)(uint32_t)(instance) << 32) | (uint64_t)(uint32_t)(stage))
#define PHRAME_STAGE_OF(id) ((uint32_t)((uint64_t)(id) & 0xffffffffu))
#define PHRAME_INSTANCE_OF(id) ((uint32_t)((uint64_t)(id) >> 32))

typedef enum
{
    pstUnknown = 0,
    pstDecode,        /* tams-player: decoded from stored media */
    pstPlayerWrite,   /* tams-player: handed to VDI */
    pstSwitch,        /* phrame_switch */
    pstComposite,     /* phrame_compositor */
    pstMix,           /* phrame_mixer_connector */
    pstOverlay,       /* phrame_overlay */
    pstEncodeSubmit,  /* phrame-webrtc-session: submitted to the encoder */
    pstPacketOut      /* phrame-webrtc-session: RTP packet emitted */
} PluginStageId;

typedef struct PluginFrameTrace
{
    uint64_t enter_ns;       /* TAI ns wall clock on entry to this stage */
    uint64_t leave_ns;       /* TAI ns wall clock on exit; 0 = never completed */
    uint64_t processing_ns;  /* CPU time expended (CLOCK_THREAD_CPUTIME_ID).
                              * NOT leave_ns - enter_ns. 0 = not measured. */
    uint64_t stage_id;       /* PHRAME_STAGE_PACK(PluginStageId, instance) */
} PluginFrameTrace;

/* ── Contributing-input identities ──────────────────────────────────────
 * A multi-input stage's output frame is a NEW frame in a NEW stream, so its own
 * frame_index is correctly its own output counter. That leaves the contributing
 * inputs' media identities with nowhere to live, and the program monitor cannot
 * say which MEDIA frame it is showing without them.
 *
 * The merge rule here is CONCATENATE, not select. That is what makes this a
 * separate mechanism from the stage trace rather than a reuse of it:
 * phrame_trace_inherit_oldest returns exactly ONE frame, which is right for a
 * trace ("a composite is only as fresh as its stalest source") and wrong for
 * identity (a composite can show two players at once, each at its own
 * origination_time, and naming one would describe half the screen).
 *
 * Fixed cap rather than a growable list, the same trade the trace makes: this
 * struct crosses a C ABI into a prebuilt libvdi.so and a hand-written Rust
 * mirror, so it has to be POD with a layout both sides can assert. Real configs
 * declare 2-4 panels; 8 is generous. Overflow sets
 * PLUGIN_INPUTS_FLAG_TRUNCATED rather than dropping silently.
 */
#define MAX_PLUGIN_FRAME_INPUTS (8)
#define PLUGIN_INPUTS_SCHEMA (1)
#define PLUGIN_INPUTS_FLAG_TRUNCATED (0x1u)

/* Set by every stage that stamps an identity table, INCLUDING when the table it
 * stamps is empty. Without it, "a participating stage whose list is legitimately
 * empty" and "a leaf frame nobody has stamped" are the same three zero fields,
 * and a consumer must guess.
 *
 * Guessing gets it wrong in one specific, damaging way. A compositor rendering
 * only its background slate with no inputs ready stamps an empty table; a reader
 * that sees "no identities" and falls back to the frame's OWN frame_index then
 * reports that stage's OUTPUT COUNTER as a media position -- the exact defect
 * this whole mechanism exists to prevent, relocated into the consumer. With this
 * flag the reader knows the answer is honestly "this stage contributed nothing
 * identifiable", and reports nothing rather than a plausible-looking lie.
 *
 * One bit in a field that already exists, so this is NOT a layout change and
 * costs nothing -- deliberately added BEFORE the ABI is republished, since doing
 * it afterwards would be another coordinated four-repo change. */
#define PLUGIN_INPUTS_FLAG_STAGE (0x2u)

/* One contributing input's media identity on a composited output frame.
 *
 * `flow` is deliberately absent: a fixed C layout cannot carry a
 * variable-length string, and the sender already knows which flow each panel
 * id carries from its own topology config. The JSON on the wire still has a
 * `flow` field -- only its source changes. */
typedef struct PluginFrameInput
{
    uint32_t id;                  /* panel / stream position in the stage's input
                                   * list. The ORIGINAL index, so it keeps naming
                                   * the same panel when a neighbour drops out. */
    uint32_t reserved;            /* explicit padding, keeps 8-byte alignment.
                                   * Named rather than implicit so both the C
                                   * static_asserts and the Rust mirror describe
                                   * the same 24 bytes. */
    uint64_t frame_index;         /* the INPUT's index in the INPUT's stream.
                                   * 0 IS A VALID INDEX -- never treat it as
                                   * "unset" (phrame-builder#15). */
    uint64_t origination_time_ns; /* TAI ns since 1970, as origination_time */
} PluginFrameInput;

typedef struct PluginFrame
{
    // DataType held in Frame
    PluginDataType data_type;

    // number of audio channels
    uint32_t channels;

    // number of audio samples per channel
    uint32_t nb_samples;

    uint32_t width;
    uint32_t height;

    // as per ColourFormat enum
    ColourFormat colour_format;

    // 1 if a alpha channel is present,
    // alpha channel has same format as
    // other components
    uint8_t has_alpha;

    // if non zero (true) indicates the data is compressed and the format indicates the compression
    uint8_t is_compressed;

    // pixel aspect ratio
    PluginRational aspect_ratio;

    // format name
    char format[32];

    // binary flags, initialy just masked with FRAME_TYPE_MASK and used to store I, B, P, can be extended, KEY FRAME also defined
    uint32_t flags;

    // underlying buffer, all data pointers will be within this
    void *buffer;

    // sizeof underlying buffer
    uint32_t buffer_size;

    // up to 4 components
    PluginComponent components[MAX_PLUGIN_PLANES];

    // as ASCIIZ string in format
    // "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
    plugin_uuid stream_uuid;

    // number of frame in stream
    uint64_t frame_index;

    // time of frames creation at source (camera etc) in TAI ns since 1970
    uint64_t origination_time;

    // time of intended use within a production in TAI ns since 1970
    // default is that it would be the same as the origination time
    uint64_t production_time;

    // number of planes active
    uint32_t planes;

    // 1 if the data is in a planar format, 0 if packed
    uint32_t is_planar;

    // example 1:25, frame_index * time_base = time stamp
    PluginRational time_base;

    // log message for tracing, store messages here
    char LogMessage[256];

    /* count of trace steps used */
    uint32_t trace_steps_used;

    /* These two occupy alignment padding that existed anyway — they are free.
     * trace_schema is asserted by BOTH C++ and the hand-written Rust mirror in
     * phrame-common-rust/src/lib.rs; libvdi.so also ships prebuilt via the
     * plugin-libs registry, so a layout disagreement is otherwise silent. */
    uint16_t trace_schema;
    uint16_t trace_flags;

    /* filled in by each step a frame is processed through */
    PluginFrameTrace trace_steps[MAX_PLUGIN_TRACE_STEPS];

    /* Contributing inputs' identities, appended by every multi-input stage.
     * See the PluginFrameInput block above for why this is a list and not the
     * single frame phrame_trace_inherit_oldest picks.
     *
     * Appended at the END of the struct on purpose: every offset above is
     * unchanged, so this is additive for anything that only reads earlier
     * fields. It is NOT free the way trace_schema/trace_flags were -- those two
     * fitted in alignment padding that existed anyway, whereas this array is
     * 192 real bytes and grows sizeof(PluginFrame) from 1040 to 1240. That
     * makes it a genuine coordinated ABI change: plugin-api, phrame-common,
     * phrame-common-rust and every consumer image must move together, and
     * libvdi.so must be rebuilt and republished to the plugin-libs registry.
     *
     * Guarded exactly the way the trace is, because those guards are the only
     * thing standing between a mismatch and silent memory corruption:
     * inputs_schema is asserted by BOTH C++ and the hand-written Rust mirror in
     * phrame-common-rust/src/lib.rs, the offsets and sizes are pinned by
     * _Static_assert in tests/abi_layout_check.c and by matching #[test]s on
     * the Rust side, and libvdi.so ships prebuilt so a disagreement is
     * otherwise silent rather than a build failure. */
    uint32_t inputs_used;
    uint16_t inputs_schema;
    uint16_t inputs_flags;
    PluginFrameInput inputs[MAX_PLUGIN_FRAME_INPUTS];

} PluginFrame;

void dump_plugin_frame(const char *message, const PluginFrame *frame, const char *LogMessage);

typedef enum
{
    prcOpenFailed = -200,
    prcDestroyFailed = -199,
    prcInstanceNotFound = -198,
    prcSmallBuffer = -197,
    prcError = -196,
    prcFrameNotReady = -195,
    prcOK = 0
} PluginReturnCode;

typedef int32_t PLUGIN_HANDLE;

/*
Open a stream
Return: a handle for the opened stream on success, error code (PluginReturnCode)
on failure

plugin_name: name of plugin to open
configuration: stream configuration
*/
typedef PLUGIN_HANDLE (*open_fn)(const char *plugin_name,
                                 const char *configuration);

/*
Check to see if source has a frame ready, 1 true 0 no

Handle: Handle for stream to access
*/
typedef PluginReturnCode (*is_frame_ready_fn)(PLUGIN_HANDLE Handle);

/*
Free allocated frame data

Handle: Handle for stream to access
frame: Pointer to frame to free
*/
typedef PluginReturnCode (*free_frame_fn)(PLUGIN_HANDLE Handle,
                                          PluginFrame *frame);

/*
Read a frame from stream

Handle: Handle for stream to access
frame: Pointer to frame to read into
*/
typedef PluginReturnCode (*read_fn)(PLUGIN_HANDLE Handle, PluginFrame *frame);

/*
Read a frame with a timeout

Handle: Handle for stream to access
frame: Pointer to frame to read into
timeout_ns: Timeout in nanoseconds
*/
typedef PluginReturnCode (*read_timeout_fn)(PLUGIN_HANDLE Handle,
                                            PluginFrame *frame,
                                            const uint64_t timeout_ns);

/*
Write frame to stream

Handle: Handle for stream to access
frame: Pointer to frame to write
*/
typedef PluginReturnCode (*write_fn)(PLUGIN_HANDLE Handle,
                                     const PluginFrame *frame);

/*
Seek to a particular point in the stream

Handle: Handle for stream to access
index: index of the stream to seek to
*/
typedef PluginReturnCode (*seek_fn)(PLUGIN_HANDLE Handle, const uint64_t index);

/*
Close the stream

Handle: Handle for stream to close
*/
typedef PluginReturnCode (*close_fn)(PLUGIN_HANDLE Handle);

/*
Get stream identifier (uuid)

Handle: Handle for stream to access
uuid: UUID to store stream ID
*/
typedef PluginReturnCode (*query_uuid_fn)(PLUGIN_HANDLE Handle, plugin_uuid *uuid);

/*
Get stream configuration
Return: Size of configuration string read on success, error code on failure

Handle: Handle for stream to access
buffer: Buffer to store configuration (Pre-allocated)
buffer_size: Size of the buffer
*/
typedef int32_t (*query_config_fn)(PLUGIN_HANDLE Handle, char *buffer,
                                   const uint32_t buffer_size);

/*
Get frames queued in stream
Return: < 0 error code, >= 0 number of frames queued
Handle: Handle for stream to access
*/
typedef int64_t (*get_queue_depth_fn)(PLUGIN_HANDLE handle);

#endif  // PLUGINS_H_
