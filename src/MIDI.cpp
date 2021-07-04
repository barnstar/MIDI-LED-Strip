

#include "MIDI.h"


void MIDIInput::parseStatusByte(MidiCommand *c, char statusByte)
{
  const uint8_t MIDI_STATUS_MASK = 0xf0;
  const uint8_t MIDI_CHANNEL_MASK = 0x0f;

  if ((statusByte & 0b10000000) == 0)
  {
    c->reset(); //'null' command
    return;
  }
  else
  {
    c->status = (int)(statusByte & MIDI_STATUS_MASK);
    c->channel = statusByte & MIDI_CHANNEL_MASK;

    //Some data sizes are deterministic.  System messages in particular
    //are of variable length.  This only indicates the minimum number of
    //data bytes to expect
    switch (c->status)
    {
    case MIDI_SYSTEM:
      c->len = 0;
      break;
    case MIDI_PROGCH:
    case MIDI_CHPRESS:
      c->len = 1;
      break;
    default:
      c->len = 2;
      break;
    }
  }
}

/*
   * Reads a single midi message and returns it.  
   */
void MIDIInput::checkMIDI(bool *didRead, MidiCommand *c)
{
  *didRead = false;

  //Read chars until we get a status char or the buffer is empty
  while (Serial.available())
  {
    uint8_t midiVal = Serial.peek();

    //Not a status byte.  Discard it.
    if ((midiVal & 0b10000000) == 0)
    {
      (void)Serial.read();
      continue;
    }

    parseStatusByte(c, midiVal);
    //Ensure the serial buffer contains the data bytes
    if (Serial.available() >= c->len)
    {
      *didRead = true;
      break;
    }
    else
    {
      //We don't have the required data bytes yet.  Bail.
      c->reset();
      return;
    }
  }

  //Pop the status byte we just peeked at.
  (void)Serial.read();

  //Read in the expected data bytes.
  //This doesn't allow for continuations so it's poor implementation
  //but it's good enough for getting NOTE_ON messages which is all
  //we really care about.
  for (uint8_t i = 0; i < c->len; i++)
  {
    c->data[i] = Serial.read();
  }
}

//Returns true if midi commands were waiting.  You can call this in a loop until it
//returns false to read queued midi events.
bool MIDIInput::updateState()
{
  bool didRead = false;
  checkMIDI(&didRead, &lastCommand);
  return didRead;
}
