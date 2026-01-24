#ifndef MIDI_PARSER_H
#define MIDI_PARSER_H

#include "midi-writer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MIDI_NOTE_OFF 0x8
#define MIDI_NOTE_ON 0x9
#define MIDI_POLY_PRESSURE 0xA
#define MIDI_CONTROLLER 0xB
#define MIDI_PROGRAM 0xC
#define MIDI_CHAN_PRESSURE 0xD
#define MIDI_PITCH_BEND 0xE

typedef struct
{
    /* could be bitfielded together, but the ordering is undefined... */
    uint8_t kind;
    uint8_t channel;

    union
    {
        struct
        {
            uint8_t note, velocity;
        } note_on, note_off;
        struct
        {
            uint8_t note, pressure;
        } poly_pressure;
        struct
        {
            uint8_t controller, value;
        } controller;
        uint8_t program;
        uint8_t chan_pressure;
        uint16_t pitch_bend;
        uint8_t bytes[2];
    } as;
} midi_event_t;

int midi_event_to_bytes (const midi_event_t *e, uint8_t *out_bytes);
int midi_event_from_bytes (midi_event_t *e, const uint8_t *bytes, uint32_t len);

typedef struct
{
    enum
    {
        EV_MIDI,
        EV_SYSEX,
        EV_META
    } kind;

    uint32_t delta;

    union
    {
        midi_event_t midi;
        struct
        {
            uint32_t length;
            const uint8_t *data;
        } sysex;
        struct
        {
            uint8_t type;
            uint32_t length;
            const uint8_t *data;
        } meta;
    } as;
} track_event_t;

typedef struct
{
    const uint8_t *bytes;
    uint32_t idx, len;
    uint8_t last_status;
} track_parser_t;

int track_event_to_bytes (const track_event_t *e, uint8_t *out_bytes);
int track_event_next (track_parser_t *p, track_event_t *e);

#ifdef MIDI_PARSER_IMPLEMENTATION

int
midi_event_to_bytes (const midi_event_t *e, uint8_t *out_bytes)
{
    int ev_len = 1, i;
    uint8_t ev_bytes[3] = { 0 };

    if (e == NULL || out_bytes == NULL) return -1;

    ev_bytes[0] |= e->kind << 4;
    ev_bytes[0] |= e->channel;

    switch (e->kind)
    {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF:
    case MIDI_POLY_PRESSURE:
    case MIDI_CONTROLLER:
        ev_bytes[1] = e->as.bytes[0];
        ev_bytes[2] = e->as.bytes[1];
        ev_len += 2;
        break;
    case MIDI_PROGRAM:
    case MIDI_CHAN_PRESSURE:
        ev_bytes[1] = e->as.bytes[0];
        ev_len += 1;
        break;
    case MIDI_PITCH_BEND:
        ev_bytes[1] = e->as.pitch_bend;
        ev_bytes[2] = (e->as.pitch_bend >> 7);
        ev_len += 2;
        break;
    default: return -1;
    }

    for (i = 0; i < ev_len; ++i) out_bytes[i] = ev_bytes[i];
    return ev_len;
}

static int
_mp_write_v (uint8_t *ptr, uint32_t v)
{
    int i = 0;

    if (ptr == NULL) return -1;

    if (v >= (1U << 28)) { ptr[i++] = ((v >> 28) & 0x7F) | 0x80; }
    if (v >= (1U << 21)) { ptr[i++] = ((v >> 21) & 0x7F) | 0x80; }
    if (v >= (1U << 14)) { ptr[i++] = ((v >> 14) & 0x7F) | 0x80; }
    if (v >= (1U << 7)) { ptr[i++] = ((v >> 7) & 0x7F) | 0x80; }
    ptr[i++] = (v & 0x7F);

    return i;
}

int
track_event_to_bytes (const track_event_t *e, uint8_t *out_bytes)
{
    int n = 0, m;

    if (out_bytes == NULL || e == NULL) return -1;

    if ((n += _mp_write_v (out_bytes, e->delta)) <= 0) return -1;

    switch (e->kind)
    {
    case EV_MIDI: return midi_event_to_bytes (&e->as.midi, out_bytes + n);
    case EV_META:
        out_bytes[n++] = e->as.meta.type;
        m = _mp_write_v (out_bytes + n, e->as.meta.length);
        if (m <= 0) return -1;
        n += m;
        memcpy (out_bytes + n, e->as.meta.data, e->as.meta.length);
        n += e->as.meta.length;
        break;
    case EV_SYSEX:
        m = _mp_write_v (out_bytes + n, e->as.sysex.length);
        if (m <= 0) return -1;
        n += m;
        memcpy (out_bytes + n, e->as.sysex.data, e->as.sysex.length);
        n += e->as.sysex.length;
        break;
    }

    return n;
}

int
midi_event_from_bytes (midi_event_t *e, const uint8_t *bytes, uint32_t len)
{
    uint8_t kind, chan;
    int bytes_used = 1;

    if (e == NULL || bytes == NULL) return -1;
    if (len < 2) return -1;

    kind = (bytes[0] & 0xF0) >> 4;
    chan = bytes[0] & 0x0F;

    switch (kind)
    {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF:
    case MIDI_POLY_PRESSURE:
    case MIDI_CONTROLLER:
        if (len < 3) return -1;
        e->as.bytes[0] = bytes[1];
        e->as.bytes[1] = bytes[2];
        bytes_used += 2;
        break;
    case MIDI_PROGRAM:
    case MIDI_CHAN_PRESSURE:
        e->as.bytes[0] = bytes[1];
        bytes_used += 1;
        break;
    case MIDI_PITCH_BEND:
        if (len < 3) return -1;
        e->as.pitch_bend = bytes[1];
        e->as.pitch_bend |= bytes[2] << 7;
        bytes_used += 2;
        break;
    }

    e->kind = kind;
    e->channel = chan;

    return bytes_used;
}

int
midi_event_from_bytes_rolling (midi_event_t *e, uint8_t status, const uint8_t *bytes, uint32_t len)
{
    uint8_t kind, chan;
    int bytes_used = 0;

    if (e == NULL || bytes == NULL) return -1;
    if (len == 0) return -1;

    kind = (status & 0xF0) >> 4;
    chan = status & 0x0F;

    switch (kind)
    {
    case MIDI_NOTE_ON:
    case MIDI_NOTE_OFF:
    case MIDI_POLY_PRESSURE:
    case MIDI_CONTROLLER:
        if (len < 2) return -1;
        e->as.bytes[0] = bytes[0];
        e->as.bytes[1] = bytes[1];
        bytes_used += 2;
        break;
    case MIDI_PROGRAM:
    case MIDI_CHAN_PRESSURE:
        e->as.bytes[0] = bytes[0];
        bytes_used += 1;
        break;
    case MIDI_PITCH_BEND:
        if (len < 2) return -1;
        e->as.pitch_bend = bytes[0];
        e->as.pitch_bend |= bytes[1] << 7;
        bytes_used += 2;
        break;
    }

    e->kind = kind;
    e->channel = chan;

    return bytes_used;
}

static int
_mp_read_v (track_parser_t *p, uint32_t offset, uint32_t *out_v)
{

    uint32_t total = 0;
    uint32_t value = 0, i;
    uint8_t b;

    if (p == NULL) return -1;
    if (p->idx + offset >= p->len) return -1;

    for (i = 0; i < 5; ++i)
    {
        b = p->bytes[p->idx + total + offset];
        total += 1;

        value = (value << 7) | (b & 0x7F);

        if ((b & 0x80) == 0) goto done;
    }

done:
    *out_v = value;
    return total;
}

int
track_event_next (track_parser_t *p, track_event_t *e)
{
    uint32_t delta;
    uint32_t bytes_left = 0;
    int32_t ev_len = 0, n = 0;
    uint8_t b;

    if (p == NULL || e == NULL) return -1;

    if ((n = _mp_read_v (p, 0, &delta)) <= 0) return 0;

    p->idx += n;
    e->delta = delta;

    bytes_left = p->len - p->idx;
    if (bytes_left == 0) return -1;

    b = p->bytes[p->idx];

    if (b >= 0x80 && b < 0xF0) /* MIDI */
    {
        if ((ev_len = midi_event_from_bytes (&e->as.midi, (const uint8_t *)p->bytes + p->idx, bytes_left)) <= 0)
            return -1;
        e->kind = EV_MIDI;
        p->last_status = b;
    }
    else if (b == 0xF0 || b == 0xF7) /* SYSEX */
    {
        uint32_t vlength;
        if ((n = _mp_read_v (p, 1, &vlength)) <= 0) return -1;

        e->kind = EV_SYSEX;
        e->as.sysex.data = (const uint8_t *)p->bytes + p->idx + 1 + n;
        e->as.sysex.length = vlength - 1;

        ev_len = 1 + n + vlength;
    }
    else if (b == 0xFF) /* META */
    {
        uint8_t type = p->bytes[p->idx + 1];
        uint32_t vlength;
        if ((n = _mp_read_v (p, 2, &vlength)) <= 0) return -1;

        e->kind = EV_META;
        e->as.meta.type = type;
        e->as.meta.data = (const uint8_t *)p->bytes + p->idx + 2 + n;
        e->as.meta.length = vlength;

        ev_len = 2 + n + vlength;
    }
    else
    {
        if (p->last_status >= 0x80 && p->last_status < 0xF0) /* rolling status */
        {
            ev_len = midi_event_from_bytes_rolling (&e->as.midi, p->last_status, (const uint8_t *)p->bytes + p->idx,
                                                    bytes_left);
            if (ev_len <= 0) return -1;
            e->kind = EV_MIDI;
        }
        else
            return -1;
    }

    p->idx += ev_len;

    return ev_len;
}

#endif /* implementation */

#endif /* include guard */
