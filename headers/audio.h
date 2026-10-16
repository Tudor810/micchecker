#ifndef AUDIO_H
#define AUDIO_H

typedef int AudioResult; // < 0: error code, >= 0: bitmask of AUDIO_INFO_* flags

enum {  // errors
    AUDIO_ERR_INVALID_ARG = -1,
    AUDIO_ERR_NO_DEVICE   = -2,
    AUDIO_ERR_FAILED      = -3,
};

enum {  // info
    AUDIO_OK                   = 0,
    AUDIO_INFO_SELECTION_RESET = 1 << 0,
    AUDIO_INFO_DEVICES_ADDED   = 1 << 1,
    AUDIO_INFO_DEVICES_REMOVED = 1 << 2,
};

#define AUDIO_SUCCEEDED(r) ((r) >= 0)
#define AUDIO_FAILED(r) ((r) < 0)

typedef struct Audio Audio;
typedef enum { AUDIO_INPUT, AUDIO_OUTPUT, AUDIO_KIND_COUNT } AudioKind;

Audio*      audio_create(void);
void        audio_destroy(Audio* a);

int         audio_device_count(const Audio* a, AudioKind kind);
const char* audio_device_name(const Audio* a, AudioKind kind, int index); // NULL if out of range
int         audio_selected(const Audio* a, AudioKind kind);

AudioResult audio_select(Audio* a, AudioKind kind, int index); 
// AUDIO_OK, or AUDIO_ERR_INVALID_ARG if index is out of rang

AudioResult audio_refresh(Audio* a, AudioKind kind); 
// < 0: error (AUDIO_ERR_*). >= 0: success, OR-ed AUDIO_INFO_* flags
//   DEVICES_ADDED / DEVICES_REMOVED: the device list changed
//   SELECTION_RESET: the selected device vanished, fell back to the default

void        console_init(void);

#endif