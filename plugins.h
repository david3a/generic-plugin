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

    // size of oen element (pixel, audio sample)
    uint32_t bytes_per_element;

} PluginComponent;

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

    // pixel aspect ratio
    PluginRational aspect_ratio;

    // format name
    char format[32];

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

    // number of bits in each component
    uint32_t bit_depth;

    // number of planes active
    uint32_t planes;

    // 1 if the data is in a planar format, 0 if packed
    uint32_t is_planar;

    // example 1:25, frame_index * time_base = time stamp
    PluginRational time_base;

    // log message for tracing, store messages here
    char LogMessage[256];

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
