# MicChecker

A small Windows command-line tool written in C for inspecting audio devices.
It uses the Windows Core Audio (MMDevice) API to list microphones and speakers,
pick which ones to use, and pick up devices that are plugged in or removed.

## Features

- List active input and output devices
- Default devices are selected automatically at startup
- Select a device by index; the selection is tracked by device ID, so it stays
  correct when the list changes
- Refresh the device list and report what changed (devices added, removed, or
  the selected device falling back to the default)

## Build

Requires a Windows toolchain such as MinGW-w64 (gcc):

```bash
gcc -Wall -Wextra -I. main.c windows/audio_win.c -o micchecker.exe -lole32 -luuid
```

## Usage

```
> list
Input devices:
  [0] Microphone Array (Realtek(R) Audio)  <- selected
Output devices:
  [0] Speakers (Realtek(R) Audio)  <- selected
> select in 1
> refresh
> exit
```

| Command | Description |
|---|---|
| `list` | show input and output devices |
| `select <in\|out> <index>` | select a device |
| `refresh` | re-scan devices and report changes |
| `help` | show this list |
| `exit` | quit |

## Project layout

- `main.c`: command loop and commands
- `headers/audio.h`: platform-independent audio API
- `windows/audio_win.c`: Windows implementation (MMDevice API)
- `headers/wav.h`, `windows/wav_win.c`: WAV support (work in progress)
