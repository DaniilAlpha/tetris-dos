#include "include/music.h"

#include <stdio.h>
#include <string.h>

#include <ctype.h>

#if (!defined __BORLANDC__)
#  include "include/borland/_null.h"
#endif

#define MAX_OCTAVE (9)

#define ROOT12_OF_2      (1.059463094)
#define SHARP_MULTIPLIER ROOT12_OF_2
#define FLAT_MULTIPLIER  (1.0 / ROOT12_OF_2)

static unsigned max_note_frequencies[7] =
    {14080, 15804, 8372, 9397, 10548, 11175, 12544};

void music_player_init(
    MusicPlayer *const self,
    unsigned const single_delay_ms,
    BeepFn const beep
) {
    self->_beep = beep;
    self->_single_delay_ms = single_delay_ms;

    self->_stream = NULL;
    self->_note_buf[0] = '\0';
}

void music_player_play(MusicPlayer *const self, FILE *const stream) {
    self->_stream = stream;
}

void music_player_next(MusicPlayer *const self) {
    unsigned char do_loop;
    unsigned beep_duration_ms, beep_frequency;

    fscanf(self->_stream, "%s", self->_note_buf);

    /* note format:
     *  - octave number: '0' to '9'
     *  - note char: 'A' to 'G'
     *  - optional flat or sharp indicator: 'b' or '#'
     *  - optional length multiplier: '.', '..', or '...'
     * space to separate from the next one
     * special notes:
     *  - pause: '-'
     *  - loop: ':'
     *  - stop: ';'
     */

    switch (self->_note_buf[0]) {
    case ';': self->_stream = NULL; return;
    case ':':
        fseek(self->_stream, 0, SEEK_SET), music_player_next(self);
        return;
    default:;
    }

    beep_duration_ms =
        self->_single_delay_ms *
        // how many '.'s + 1
        (strchr(self->_note_buf, '.')
             ? 2 + strrchr(self->_note_buf, '.') - strchr(self->_note_buf, '.')
             : 1);

    if (self->_note_buf[0] == '-') {
        beep_frequency = 0;
    } else {
        beep_frequency = max_note_frequencies[toupper(self->_note_buf[1]) - 'A']
                      // shift by the octave difference
                      >> (MAX_OCTAVE - (self->_note_buf[0] - '0'));

        switch (self->_note_buf[2]) {
        case '#': beep_frequency *= SHARP_MULTIPLIER; break;
        case 'b': beep_frequency *= FLAT_MULTIPLIER; break;
        default:;
        }
    }

    self->_beep(beep_frequency, beep_duration_ms);
}
