#include "MIDI.h"

const uint8_t MIDI_STATUS_MASK = 0xf0;
const uint8_t MIDI_CHANNEL_MASK = 0x0f;
const uint8_t MIDI_STATUS_BYTE_MASK = 0xb10000000;

void MIDIInput::start() 
{
  Serial.begin(31250);
}

void MIDIInput::parseStatusByte(char statusByte)
{
  statusMsg.status = (int)(statusByte & MIDI_STATUS_MASK);
  statusMsg.channel = statusByte & MIDI_CHANNEL_MASK;

  //Some data sizes are deterministic.  System messages in particular
  //are of variable length.  This only indicates the minimum number of
  //data bytes to expect
  switch (statusMsg.status)
  {
  case MIDI_SYSTEM:
    statusMsg.len = 0;
    break;
  case MIDI_PROGCH:
  case MIDI_CHPRESS:
    statusMsg.len = 1;
    break;
  default:
    statusMsg.len = 2;
    break;
  }
}

/*
   * Reads a single midi message and returns it.  
   */
void MIDIInput::readPendingEvent(bool *didRead)
{
  *didRead = false;

  //Read chars until we get a status char or the buffer is empty
  while (Serial.available())
  {
    uint8_t midiVal = Serial.peek();

    //Not a status byte.  Discard it.
    if ((midiVal & MIDI_STATUS_BYTE_MASK) == 0)
    {
      (void)Serial.read();
      continue;
    }

    parseStatusByte(midiVal);
    //Ensure the serial buffer contains the data bytes
    if (Serial.available() >= statusMsg.len)
    {
      *didRead = true;
      break;
    }
    else
    {
      //We don't have the required data bytes yet.  Bail.
      statusMsg.reset();
      return;
    }
  }

  //Pop the status byte we just peeked at.
  (void)Serial.read();

  //Read in the expected data bytes.
  //This doesn't allow for continuations so it's poor implementation
  //but it's good enough for getting NOTE_ON messages which is all
  //we really care about.
  for (uint8_t i = 0; i < statusMsg.len; i++)
  {
    statusMsg.data[i] = Serial.read();  
   }
}

//Returns true if midi commands were waiting.  You can call this in a loop until it
//returns false to read queued midi events.
bool MIDIInput::readNextPendingEvent()
{
  bool didRead = false;
  readPendingEvent(&didRead);
  return didRead;
}
