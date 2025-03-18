#ifndef MUSIC_H
#define MUSIC_H

#include <stdio.h>

typedef void (*BeepFn)(unsigned frequency, unsigned duration_ms);

typedef struct MusicPlayer {
    BeepFn _beep;

    unsigned _single_delay_ms;

    FILE *_stream;

    char _note_buf[8];
} MusicPlayer;

void music_player_init(
    MusicPlayer *const self,
    unsigned const single_delay_ms,
    BeepFn const beep
);
void music_player_play(MusicPlayer *const self, FILE *const stream);
void music_player_next(MusicPlayer *const self);

#endif
