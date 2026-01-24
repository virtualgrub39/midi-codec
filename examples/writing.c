#define MIDI_PARSER_IMPLEMENTATION
#include <midi-parser.h>
#define MIDI_WRITER_IMPLEMENTATION
#include <midi-writer.h>

#include <stdio.h>

int
main (void)
{
    const char *path = "output.mid";
    FILE *midif;
    midi_writer_t mw = { 0 };
    uint8_t notes[] = { 60, 64, 67 };
    int num_notes = 3;
    int j;

    midif = fopen (path, "wb");

    mw_begin (&mw, midif, MIDI_FMT_SINGLE, 96);

    mw_track_begin (&mw);

    /* example using semi-manual encoding, with rolling-status */
    for (j = 0; j < num_notes; ++j)
    {
        midi_event_t ev;
        uint8_t buf[8];
        int n = 0;

        ev.kind = MIDI_NOTE_ON;
        ev.channel = 0;
        ev.as.note_on.note = notes[j];
        ev.as.note_on.velocity = 100;

        n += midi_vlq_encode (0, buf);
        n += midi_event_to_bytes (&ev, buf + n, j > 0);
        mw_track_append (&mw, buf, n);
    }

    /* example using fancy do-everything `track_event_to_bytes`, but no rolling-status */
    for (j = 0; j < num_notes; ++j)
    {
        track_event_t ev = { 0 };
        uint8_t buf[8];

        ev.delta = (j == 0) ? 480 : 0;

        ev.kind = EV_MIDI;
        ev.as.midi.kind = MIDI_NOTE_ON;
        ev.as.midi.channel = 0;
        ev.as.midi.as.note_on.note = notes[j];
        ev.as.midi.as.note_on.velocity = 0;

        track_event_to_bytes (&ev, buf);
        mw_track_append (&mw, buf, track_event_get_storage_size (&ev));
    }

    {
        track_event_t footer = { 0 };
        uint8_t bytes[4];
        footer.delta = 0;
        footer.kind = EV_META;
        footer.as.meta.type = 0x2F;
        footer.as.meta.length = 0;
        track_event_to_bytes (&footer, bytes);
        mw_track_append (&mw, bytes, 4);
    }

    mw_track_end (&mw);

    mw_end (&mw);

    fclose (midif);
    return 0;
}