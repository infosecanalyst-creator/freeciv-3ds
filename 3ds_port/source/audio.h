#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// Sound effects (Freeciv's stdsounds) and music, through the 3DS DSP.
// Everything quietly does nothing if the DSP can't start (no dspfirm.cdc).

void audio_init(void);
void audio_exit(void);
// Call once per frame: keeps the music stream fed.
void audio_update(void);

// Play a Freeciv sound tag, e.g. "m_settlers", "f_catapult", "e_tech_gain".
// Unit tags fall back to their generic form ("m_generic").
void audio_play(const char *tag);

typedef enum { MUSIC_NONE, MUSIC_MENU, MUSIC_GAME } MusicMode;
void audio_set_music(MusicMode mode);

// Volumes 0..10.
void audio_set_volumes(int music, int sfx);
int  audio_music_volume(void);
int  audio_sfx_volume(void);
bool audio_available(void);

#endif
