#define MIDI_PARSER_IMPLEMENTATION
#include <midi-parser.h>
#define MIDI_READER_IMPLEMENTATION
#include <midi-reader.h>

#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
    const char *path = "output.mid";
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
        track_parser_t tp = { 0 };
        track_event_t ev = { 0 };

        mr_get_track_data (&mr, evdata);

        printf ("----- Track %03d -----\n", mr.track_idx + 1);
        printf ("[%d bytes]\n", tracklen);

        tp.bytes = evdata;
        tp.idx = 0;
        tp.len = tracklen;

        while (track_event_next (&tp, &ev) > 0)
        {
            printf ("<%04u> [", ev.delta);
            switch (ev.kind)
            {
            case EV_MIDI:
                printf ("MIDI] { ");
                printf ("k: %1X c: %1X ", ev.as.midi.kind, ev.as.midi.channel);
                printf ("d: %02X %02X ", ev.as.midi.as.bytes[0], ev.as.midi.as.bytes[1]);
                /* You can extract different MIDI event data from ev.as.midi.as. ...   */
                printf ("}\n");
                break;
            case EV_META:
                printf ("META] { ");
                printf ("k: %02X l: %07X ", ev.as.meta.type, ev.as.meta.length);
                /* data in: ev.as.meta.data */
                printf ("}\n");
                break;
            case EV_SYSEX:
                printf ("SYSEX] { ");
                printf ("l: %07X ", ev.as.sysex.length);
                /* data in: ev.as.sysex.data */
                printf ("}\n");
            }
        }

        free (evdata);
    }

    fclose (midif);
    return 0;
}
