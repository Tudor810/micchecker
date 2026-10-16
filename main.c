#include <stdio.h>
#include <string.h>

#include "headers/audio.h"

#define LINE_LEN 256

typedef struct {
    int    running;
    Audio* audio; 
} App;


/* ---- commands ---- */

static const char* kind_label[AUDIO_KIND_COUNT] = {"Input", "Output"};

static void cmd_help(App* app, const char* args);

static void cmd_list(App* app, const char* args) {
    (void)args;
    for(int k = 0; k < AUDIO_KIND_COUNT;k++) {
        printf("%s devices:\n", kind_label[k]);

        int n = audio_device_count(app->audio, k);
        if(n == 0) printf("  (none)\n");
        for(int i = 0; i < n;i++) {
            printf("  [%d] %s%s\n", i, audio_device_name(app->audio, k, i),
                   i == audio_selected(app->audio, k) ? "  <- selected" : "");
        }
    }
}

static void cmd_select(App* app, const char* args) {
    char kind[8];
    int idx;
    if (sscanf(args, "%7s %d", kind, &idx) != 2) {
        printf("usage: select <in|out> <index>\n");
        return;
    }

    AudioKind k;
    if      (strcmp(kind, "in")  == 0) k = AUDIO_INPUT;
    else if (strcmp(kind, "out") == 0) k = AUDIO_OUTPUT;
    else { printf("unknown kind '%s' (use in or out)\n", kind); return; }

    if (AUDIO_FAILED(audio_select(app->audio, k, idx))) {
        printf("invalid index\n");
        return;
    }

    printf("%s device set to: %s\n", kind_label[k], audio_device_name(app->audio, k, idx));
}

static void cmd_refresh(App* app, const char* args) {
    (void)args;
    for (int k = 0; k < AUDIO_KIND_COUNT; k++) {
        AudioResult r = audio_refresh(app->audio, k);
        if (AUDIO_FAILED(r)) { 
            printf("%s: refresh failed\n", kind_label[k]); 
            continue; 
        }
        if (r & AUDIO_INFO_DEVICES_ADDED)   printf("%s: device(s) added\n", kind_label[k]);
        if (r & AUDIO_INFO_DEVICES_REMOVED) printf("%s: device(s) removed\n", kind_label[k]);
        if (r & AUDIO_INFO_SELECTION_RESET) printf("%s device removed, now using default\n", kind_label[k]);
    }
    cmd_list(app, NULL);
}

static void cmd_exit(App* app, const char* args) {
    (void)args;
    app->running = 0;
}

typedef struct {
    const char *name;
    const char *help;
    void (*run)(App* app, const char *args);
} Command;

static const Command commands[] = {
    { "list",   "show input and output devices", cmd_list   },
    { "select", "select <in|out> <index>",       cmd_select },
    { "refresh", "refresh device list",          cmd_refresh},
    { "help",   "show this list",                cmd_help   },
    { "exit",   "quit",                          cmd_exit   },
};

#define N_COMMANDS (sizeof commands / sizeof commands[0])

static void cmd_help(App* app, const char* args) {
    (void)app;
    (void)args;
    for(size_t i = 0;i < N_COMMANDS;i++) {
        printf("  %-8s %s\n", commands[i].name, commands[i].help);
    }
}

static void dispatch(App* app, char* line) {
    char* cmd = line + strspn(line, " \t"); // Handling white spaces
    if (*cmd == '\0') return; // Empty command

    char* args = cmd + strcspn(cmd, " \t"); 
    
    if(*args) {
        *args++ = '\0';
        args += strspn(args, " \t");
    }

    for(size_t i = 0; i < N_COMMANDS; i++) {
        if(strcmp(commands[i].name, cmd) == 0) {
            commands[i].run(app, args);
            return;
        }
    }
    printf("unknown command '%s' (try 'help')\n", cmd);
}

int main(void) {
    console_init();
    App app_state = {0};

    app_state.audio = audio_create();
    app_state.running = 1;

    if(!app_state.audio) {
        fprintf(stderr, "failed to initialize audio\n");
        return 1;
    }

    char line[LINE_LEN];

    while(app_state.running) {
        printf("> ");
        fflush(stdout);

        if(!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        dispatch(&app_state, line);
    }

    audio_destroy(app_state.audio);
    return 0;
}