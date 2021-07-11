#ifndef MIDI_h
#define MIDI_h

#include <Arduino.h>

enum MIDIStatus
{
    MIDI_NONE = 0x00,
    MIDI_NOTE_OFF = 0x80,
    MIDI_NOTE_ON = 0x90,
    MIDI_POLY = 0xa0,
    MIDI_CONTROL = 0xb0,
    MIDI_PROGCH = 0xc0,
    MIDI_CHPRESS = 0xd0,
    MIDI_PITCHBEND = 0xe0,
    MIDI_SYSTEM = 0xf0,
};

enum MIDIControl
{
    MIDI_CTL_NONE = 0x00,
    MIDI_CTL_EXP = 0x0B,
    MIDI_CTL_SUSTAIN = 0x40,
};

enum MIDIValues {
  SUSTAIN_ON = 64
};

class MidiStatusMessage
{
public:
    MIDIStatus status = MIDI_NONE;
    uint8_t channel = 0;
    uint8_t data[2] = {0, 0};
    uint8_t len = 0;

    void reset()
    {
        status = MIDI_NONE;
        channel = 0;
        memset(data, 0, 2);
        len = 0;
    }

    uint8_t velocity() { return data[1]; }
    uint8_t pitch()    { return data[0]; }
};

class MIDIInput
{
public:
    //The last status message we read.  You can call reset on this to indicate that
    //the message was consumed.  This should never have a status of MIDI_NONE 
    //after readNextPendingEvent() returns true;
    MidiStatusMessage statusMsg;

    //Indicates if the sustain pedal is "on"
    bool sustainOn = false;

    //Indicates the expression pedal level
    uint8_t expressionLevel = 127;

    void start();

    //Returns true if midi commands were waiting.  You can call this in a loop until it
    //returns false to read queued midi events.
    bool readPendingEvent();

private:
    void parseStatusByte(uint8_t statusByte);
};

#endif
