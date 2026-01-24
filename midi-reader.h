/* MIDI-reader - a simple MIDI file parser
 * Copyright (c) 2026, virtualgrub39
 * All rights reserved.
 * This header can be used, to parse MIDI file header, and extract track data out of the MIDI file.
 * It's made to be easily readable and hackable, use the simplest API possible, and be easy to embed into existing
 projects.
 * Safety is not the main concern though, so You should know what you're doing, when using this library.

 * Example usage

 ```c
 #define MIDI_READER_IMPLEMENTATION
 #include "midi-reader.h"

 #include <stdio.h>
 #include <stdlib.h>

 int
 main (void)
 {
     const char *path = "melody.mid";
     FILE *midif;
     midi_reader_t mr = { 0 };
     uint32_t tracklen;

     midif = fopen (path, "rb");
     mr_begin (&mr, midif);

     printf ("File: %s\n", path);
     printf ("Format: %hu\n", mr.format);
     printf ("Track count: %hu\n", mr.ntracks);
     printf ("Timing interval: %hu\n", mr.tickdiv);

     while ((tracklen = mr_next_track (&mr)) > 0)
     {
         unsigned char *evdata = malloc (tracklen);
         mr_get_track_data (&mr, evdata);

         printf ("----- Track %03d -----\n", mr.track_idx + 1);
         printf ("[%d bytes]\n", tracklen);

         // ... parse events using midi-parser.h :3 ...

         free (evdata);
     }

     fclose (midif);
     return 0;
 }
 ```

 * License

 Copyright (c) 2026, virtualgrub39
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#ifndef MIDI_READER_H
#define MIDI_READER_H

#include <stdint.h>
#include <stdio.h>

/* This structure MUST be zero-initialized before use */
typedef struct
{
    /* parser state */
    FILE *src;   /* source file */
    uint32_t i;  /* current file offset */
    int eotrack; /* 1 - end of track reached; 0 otherwise */
    int eof;     /* 1 - end of file reached; 0 otherwise */
    /* header info */
    uint16_t ntracks; /* number of tracks in file */
    uint16_t format;  /* file format */
    uint16_t tickdiv; /* timing interval */
    /* track info */
    int track_idx;      /* current track index */
    uint32_t track_len; /* length of current track in bytes */
} midi_reader_t;

/* Initializes MIDI reader context; Tries to parse MIDI header;
 * On success fills reader context with header info and returns 0;
 * On any failure (couldn't read from source file, invalid header, etc.) returns non-0 value; No error codes are set
 * anywhere. You may check `errno` for any system errors, but it's not guaranteed to be set; */
int mr_begin (midi_reader_t *mr, FILE *src);

/* Reads from source file, untill track marker is encoutered; Parses track length in bytes;
 * On failure (reached eof before finding marker, etc.) returns 0;
 * On success returns track length in bytes (non-0 for any valid MIDI file); */
uint32_t mr_next_track (midi_reader_t *mr);

/* Reads all event data from the current track;
 * On failure (reached end of track / eof, malformed event, etc.) returns non-zero value;
 * On success, `out_data` is filled with event data and it's length, and 0 is returned.
 * `out_data` is expected to be pre-allocated by the user to correct size. */
int mr_get_track_data (midi_reader_t *mr, uint8_t *out_data);

/* Finalizes all reads, leaving the source file in usable state.
 * This function DOESN'T close the file provided in `mr_init`. */
void mr_end (midi_reader_t *mr);

#ifdef MIDI_READER_IMPLEMENTATION

static int
_mr_read_u32 (midi_reader_t *mr, uint32_t *out_u32)
{
    uint8_t b[4];
    uint32_t n = fread (b, 1, 4, mr->src);
    mr->i += n;
    if (n != 4)
    {
        if (feof (mr->src)) mr->eof = 1;
        return n - 4;
    }
    *out_u32 = b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
    return n - 4;
}

static int
_mr_read_u16 (midi_reader_t *mr, uint16_t *out_u16)
{
    uint8_t b[2];
    uint32_t n = fread (b, 1, 2, mr->src);
    mr->i += n;
    if (n != 2)
    {
        if (feof (mr->src)) mr->eof = 1;

        return n - 2;
    }
    *out_u16 = b[0] << 8 | b[1];
    return n - 2;
}

static int
_mr_read_u8 (midi_reader_t *mr, uint8_t *out_u8)
{
    uint8_t b;
    uint32_t n = fread (&b, 1, 1, mr->src);
    mr->i += n;
    if (n != 1)
    {
        if (feof (mr->src)) mr->eof = 1;

        return n - 1;
    }
    *out_u8 = b;
    return n - 1;
}

int
mr_begin (midi_reader_t *mr, FILE *src)
{
    uint32_t magic;
    uint32_t header_len;

    if (mr == NULL) return -1;

    mr->src = src;
    mr->i = 0;
    mr->eof = 0;
    mr->eotrack = 0;
    mr->track_idx = -1;

    /* magic (const 0x4d546864) */
    if (_mr_read_u32 (mr, &magic) != 0) goto error;
    if (magic != 0x4d546864) goto error;

    /* header length (const 6) */
    if (_mr_read_u32 (mr, &header_len) != 0) goto error;
    if (header_len != 6) goto error;

    if (_mr_read_u16 (mr, &mr->format) != 0) goto error;  /* format */
    if (_mr_read_u16 (mr, &mr->ntracks) != 0) goto error; /* ntracks */
    if (_mr_read_u16 (mr, &mr->tickdiv) != 0) goto error; /* tickdiv */

    return 0;
error:
    return -1;
}

uint32_t
mr_next_track (midi_reader_t *mr)
{
    char b[4] = { 0 };
    uint8_t c;
    uint32_t track_len;

    if (mr == NULL) return 0;

    for (;;)
    {
        if (_mr_read_u8 (mr, &c) != 0) return 0;
        if (mr->eof) return 0;

        b[0] = b[1];
        b[1] = b[2];
        b[2] = b[3];
        b[3] = c;

        if ((uint32_t)(b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]) == 0x4D54726B) break;
    }

    if (_mr_read_u32 (mr, &track_len) != 0) return 0;

    mr->track_idx += 1;
    mr->track_len = track_len;

    return track_len;
}

int
mr_get_track_data (midi_reader_t *mr, uint8_t *out_data)
{
    uint8_t c;
    uint32_t i;

    if (mr == NULL) return -1;
    if (mr->eotrack) return -1;
    if (mr->eof) return -1;
    if (mr->track_len == 0) return -1;

    for (i = 0; i < mr->track_len; ++i)
    {
        if (_mr_read_u8 (mr, &c) != 0) return -1;
        if (out_data) out_data[i] = c;
    }

    return 0;
}

void
mr_end (midi_reader_t *mr)
{
    if (mr == NULL) return;
    (void)mr;
}

#endif /* implementation */

#endif /* include guard */
