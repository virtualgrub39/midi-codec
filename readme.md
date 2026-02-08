# midi-codec

Collection of single-header MIDI utilities written in C89

[midi-reader](midi-reader.h) is a MIDI file reader, capable of parsing file header, and extracting track data.

[midi-writer](midi-writer.h) is a MIDI file writer, capable of creating MIDI file header, appending track headers and arbitrary data.

[midi-parser](midi-parser.h) is a general MIDI event serializer/deserializer. It's capable of creating and serializing any MIDI, META and SYSEX event.

`midi-reader` and `midi-writer` are designed for single-pass reading and writing. There is no option to jump between tracks or events.

None of those is a super optimized demon of speed, but they are simple, and do work fine.

## usage

To use any of the headers in Your project, just:

```c
// midi-reader
#define MIDI_READER_IMPLEMENTATION
#include "midi-reader.h"

// midi-writer
#define MIDI_WRITER_IMPLEMENTATION
#include "mini-writer.h"

// midi-parser
#define MIDI_PARSER_IMPLEMENTATION
#include "midi-parser.h"
```

the implementation macro (`#define MIDI_..._IMPLEMENTATION`) should be written only once in your whole project.

For more info see headers themselves - there should be some comments. If not - read source code - it's very simple. There are also some usage examples in [examples](examples) directory.

## license

see [LICENSE](LICENSE)
