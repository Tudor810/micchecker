#ifndef AUDIO_H
#define AUDIO_H


typedef struct Audio Audio;
typedef enum { AUDIO_INPUT, AUDIO_OUTPUT, AUDIO_KIND_COUNT } AudioKind;


Audio*      audio_create(void);
void        audio_destroy(Audio* a);

int         audio_device_count(const Audio* a, AudioKind kind);
const char* audio_device_name(const Audio* a, AudioKind kind, int index); // NULL if out of range
int         audio_selected(const Audio* a, AudioKind kind);
int         audio_select(Audio* a, AudioKind kind, int index); // 0 succes, -1 invalid index

int         audio_refresh(Audio* a);

void        console_init(void);
#endif