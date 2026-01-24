#ifndef MIDI_WRITER_H
#define MIDI_WRITER_H

#include <stdint.h>
#include <stdio.h>

typedef struct
{
    FILE *dst;
    uint32_t i;
    /* header info */
    uint16_t ntracks;
    /* track info */
    uint32_t track_offset;
} midi_writer_t;

int mw_begin (midi_writer_t *mw, FILE *dst, int16_t format, uint16_t tickdiv);
int mw_end (midi_writer_t *mw);
int mw_track_begin (midi_writer_t *mw);
int mw_track_append (midi_writer_t *mw, const uint8_t *data, uint32_t len);
int mw_track_end (midi_writer_t *mw);

#ifdef MIDI_WRITER_H

static int
_mw_write_u32 (midi_writer_t *mw, uint32_t u32)
{
    uint8_t b[4];
    uint32_t n;

    b[0] = u32 >> 24;
    b[1] = u32 >> 16;
    b[2] = u32 >> 8;
    b[3] = u32;

    n = fwrite (b, sizeof *b, 4, mw->dst);
    mw->i += n;
    return n - 4;
}

static int
_mw_write_u16 (midi_writer_t *mw, uint16_t u16)
{
    uint8_t b[2];
    uint32_t n;

    b[0] = u16 >> 8;
    b[1] = u16;

    n = fwrite (b, sizeof *b, 2, mw->dst);
    mw->i += n;
    return n - 2;
}

int
mw_begin (midi_writer_t *mw, FILE *dst, int16_t format, uint16_t tickdiv)
{
    if (!mw) return -1;
    if (!dst) return -1;

    mw->dst = dst;
    mw->i = 0;
    mw->ntracks = 0;

    if (_mw_write_u32 (mw, 0x4d546864) != 0) return -1; /* magic */
    if (_mw_write_u32 (mw, 6) != 0) return -1;          /* header length */
    if (_mw_write_u16 (mw, format) != 0) return -1;     /* header length */
    if (_mw_write_u16 (mw, 0xAFAF) != 0) return -1;     /* ntracks (placeholder) */
    if (_mw_write_u16 (mw, tickdiv) != 0) return -1;    /* tickdiv */

    return 0;
}

int
mw_track_begin (midi_writer_t *mw)
{
    if (!mw) return -1;

    if (_mw_write_u32 (mw, 0x4d54726b) != 0) return -1; /* magic */
    if (_mw_write_u32 (mw, 0xFAFAFAFA) != 0) return -1; /* track_len (placeholder) */

    mw->track_offset = mw->i;

    return 0;
}

int
mw_track_append (midi_writer_t *mw, const uint8_t *data, uint32_t len)
{
    if (!mw) return -1;
    if (!data || len == 0) return -1;

    if (fwrite (data, sizeof *data, len, mw->dst) != len)
    {
        fseek (mw->dst, mw->i, SEEK_SET);
        return -1;
    }
    mw->i += len;

    return 0;
}

int
mw_track_end (midi_writer_t *mw)
{
    uint32_t track_len;
    size_t saved_i;

    if (!mw) return -1;

    saved_i = mw->i;

    if (fseek (mw->dst, mw->track_offset - 4, SEEK_SET) != 0) return -1;

    track_len = saved_i - mw->track_offset;

    if (_mw_write_u32 (mw, track_len) != 0)
    {
        fseek (mw->dst, saved_i, SEEK_SET);
        return -1;
    }

    mw->i = saved_i;
    if (fseek (mw->dst, saved_i, SEEK_SET) != 0) return -1;
    mw->ntracks += 1;

    return 0;
}

int
mw_end (midi_writer_t *mw)
{
    if (!mw) return -1;
    if (mw->dst)
    {
        fseek (mw->dst, 10, SEEK_SET);
        if (_mw_write_u16 (mw, mw->ntracks) != 0) return -1;
    }
    return 0;
}

#endif /* implementation */

#endif /* include guard */
