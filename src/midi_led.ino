#include <Adafruit_NeoPixel.h>
        
#define LED_CONTROL_PIN 13               //The LED control pin
#define LED_COUNT 60                     //The number of LEDS in your strip. 
#define LEDS_PER_PITCH (LED_COUNT/ 72.0) //A typical 60 LED strip covers 72 keys
#define LED_START_PITCH 24               //Pitches below this will be ignored

//Arduino needs types defined up front
struct Pixel;
struct PixelFilter;
struct MidiCommand;

/******************************************************************
 * MIDI
 ******************************************************************/

enum MIDIStatus {
  MIDI_NONE     = 0x00,
  MIDI_NOTE_OFF = 0x80,
  MIDI_NOTE_ON  = 0x90,
  MIDI_POLY     = 0xa0,
  MIDI_CONTROL  = 0xb0,
  MIDI_PROGCH   = 0xc0,
  MIDI_CHPRESS  = 0xd0,
  MIDI_PITCHBEND= 0xe0,
  MIDI_SYSTEM   = 0xf0,   
};

enum MIDIControl {
  MIDI_CTL_NONE    = 0x00,
  MIDI_CTL_EXP     = 0x0B,
  MIDI_CTL_SUSTAIN = 0x40,
};

#define SUSTAIN_ON 64

typedef struct MidiCommand {
  MIDIStatus status = MIDI_NONE;
  uint8_t channel = 0;
  uint8_t data[2] = {0,0};
  uint8_t len = 0;

  void reset() 
  {
    status = MIDI_NONE;
    channel = 0;
    memset(data, 0, 2);
    len = 0;
  }
  
} MidiCommand;

struct  {
  MidiCommand lastCommand;
  bool sustainOn = false;
  uint8_t expressionLevel = 127;
  
  void parseStatusByte(MidiCommand *c, char statusByte) 
  {
    const uint8_t MIDI_STATUS_MASK = 0xf0;
    const uint8_t MIDI_CHANNEL_MASK = 0x0f;
    
    if ( ( statusByte & 0b10000000 ) == 0 ) {
      c->reset(); //'null' command
      return;
    } else {
      c->status = (int)(statusByte & MIDI_STATUS_MASK);
      c->channel = statusByte & MIDI_CHANNEL_MASK;
  
      //Some data sizes are deterministic.  System messages in particular
      //are of variable length.  This only indicates the minimum number of
      //data bytes to expect
      switch(c->status) {
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
  void checkMIDI(bool *didRead, MidiCommand *c)
  {
    *didRead = false;
  
    //Read chars until we get a status char or the buffer is empty
    while(Serial.available()) {
      uint8_t midiVal = Serial.peek();
  
      //Not a status byte.  Discard it.
      if ((midiVal & 0b10000000) == 0) {
        (void)Serial.read();
        continue;
      }
  
      parseStatusByte(c, midiVal);
      //Ensure the serial buffer contains the data bytes
      if (Serial.available() >= c->len) {
        *didRead = true;
        break; 
      } else {
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
    for (uint8_t i=0; i<c->len; i++) {
      c->data[i] = Serial.read();
    }
  }


  //Returns true if midi commands were waiting.  You can call this in a loop until it
  //returns false to read queued midi events.
  bool updateState() {
    bool didRead = false;
    checkMIDI(&didRead, &lastCommand);
    return didRead;
  }
  
} MIDIState;


/******************************************************************
 * LED STRIP DISPLAY
 *****************************************************************/
typedef struct Pixel {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t w = 0;

  void fade(float val) {
    r *= val;
    g *= val;
    b *= val;
    w *= val;
  }

  uint32_t pack() {
     return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
  }

  void combine(Pixel *p, float ratio) {
    r = min(uint8_t(r + (p->r * ratio)), 255);
    g = min(uint8_t(g + (p->g * ratio)), 255);
    b = min(uint8_t(b + (p->b * ratio)), 255);
  }

  void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    this->r = r; this->g = g; this->b = b;
  }

  void setRGB(uint8_t *c) {
    this->r = (*c); this->g = *(c+1); this->b = *(c+2);
  }

  void copy(Pixel *p) {
    this->r = p->r; this->g = p->g; this->b = p->b;
  }

} Pixel; 

typedef struct PixelFilter {
  const short len;
  float filter[3];
} PixelFilter;

uint8_t colorIndex = 0;
#define COLOR_COUNT 6
/* Adds a random value to a random pixel in the given buffer */
Pixel dripVal;
Pixel* drip(Pixel *p, double intensity) 
{
    const char colors[COLOR_COUNT][3] = { {255,0,0}, 
                              {0,255,0}, 
                              {0,0,255},
                              {255,255,0},
                              {0,255,255},
                              {255,0,255}};

    colorIndex++;
    if(colorIndex == COLOR_COUNT)colorIndex=0;
    dripVal.setRGB(&(colors[colorIndex][0]));
    dripVal.fade(intensity);
    p->combine(&dripVal, 1.0);
    return &dripVal;
}

/* Runs a low pass filter over backBuffer and stores the results in renderBuffer */
void blur(Pixel *renderBuffer, Pixel *backBuffer, size_t size, PixelFilter *filter) 
{
    uint16_t halfLen = (filter->len) / 2;
    for(uint16_t i=0; i<size; i++) {
      Pixel *dst = renderBuffer + i;
      Pixel val; 
      for(uint16_t f=0; f<filter->len; f++) {
        int16_t pos = i+(f-halfLen);
        if(pos<0 || pos>=size)continue;
        Pixel *src = backBuffer + pos;
        val.combine(src, filter->filter[f]);
      }
      *dst = val;
  }
}

/* Renders the given buffer to the LED strip */
void render(Pixel* buffer,  size_t size, Adafruit_NeoPixel *strip) 
{
  for(uint16_t i=0; i<size; i++) {
      Pixel *p = buffer + i;
      strip->setPixelColor(i,p->pack());
  }
  strip->show();
}


/********************************************************************
 * Arduino Runloop
 * ******************************************************************/

PixelFilter blur1 = { 3, {0.1, .65, 0.1} };

Pixel bufferA[LED_COUNT];
Pixel bufferB[LED_COUNT];

Pixel *renderBuffer = bufferA;
Pixel *backBuffer = bufferB;

/* Swaps the render and back buffers */
void swapBuffers()
{
  Pixel *tmp = renderBuffer;
  renderBuffer = backBuffer;
  backBuffer = tmp;
}

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_CONTROL_PIN, NEO_GRB + NEO_KHZ800);
Pixel activeNotes[LED_COUNT];

void setup()
{
  //Starts the MIDI interface
  Serial.begin(31250);

  strip.begin();
  strip.show(); 
}

#define PIXEL_OUT_OF_RANGE -1



void loop()
{
  MidiCommand *c = &(MIDIState.lastCommand);

  bool didRead = MIDIState.updateState();
  //Read a note-on and the velocity is not zero
  if(didRead) {
    if ( (c->status == MIDI_NOTE_ON) && (c->data[1] != 0) ) {
      //data[0] for NOTE_ON is the pitch.  data[1] is the velocity
      uint16_t position = pitchToPixelPosition(c->data[0]);   
      if (position != PIXEL_OUT_OF_RANGE) {
        Pixel *p = backBuffer + position;
        double velocity = c->data[1];
        //Compress the the intensity a bit
        double intensity = (0.5 + (velocity / 127.0))/1.5;
        Pixel *dripVal = drip(p, intensity); 

        //The active notes array is use to control the sustain duration
        //The constants are all chosen to keep a nice balance between
        //busy and not too busy.  The values are all dependent on the
        //loop speed.  Adjust to taste.

        activeNotes[position].copy(dripVal); 
        activeNotes[position].fade(0.4);  //Magic!
      }
    }
    else if ((c->status == MIDI_NOTE_OFF) || 
             ((c->status == MIDI_NOTE_ON) && c->data[1] == 0)) {
        uint16_t position = pitchToPixelPosition(c->data[0]);
        activeNotes[position].setRGB(0,0,0);
    } 
    else if (c->status == MIDI_CONTROL) {
      uint8_t controlType = c->data[0];
      uint8_t controlValue = c->data[1];

      switch(controlType) {
        case MIDI_CTL_SUSTAIN: { 
          switch (controlValue) {
           case SUSTAIN_ON: 
              MIDIState.sustainOn = true;
              break;
           default:
              MIDIState.sustainOn = false;
              break;
          }
          break;
        }
        case MIDI_CTL_EXP: {
          MIDIState.expressionLevel = c->data[1];
        }
        default:
          break;
       }
    }
  }

  for(int i=0; i<LED_COUNT; i++) {
    double fadeVal = MIDIState.sustainOn ? 0.99 : 0.97;
    activeNotes[i].fade(fadeVal);   //More magic.  This is quite sensitive.
    Pixel *p = backBuffer + i;
    p->combine(activeNotes+i,1.0);
  }

  blur(renderBuffer, backBuffer, LED_COUNT, &blur1);
  render(renderBuffer, LED_COUNT, &strip);
  swapBuffers();
}


/* Maps a pitch value to a pixel using the LED_START_PITCH,
 * and LEDS_PER_PIXEL, and LED_COUNT constants
 */
uint16_t pitchToPixelPosition(uint8_t pitch) 
{
  int offsetPitch = pitch - LED_START_PITCH;
  if(offsetPitch < 0) {
    return PIXEL_OUT_OF_RANGE; 
  }
  
  
  double position = (double)offsetPitch * LEDS_PER_PITCH;
  if (position >= LED_COUNT) {
    return PIXEL_OUT_OF_RANGE;
  }

  return (uint16_t)position;
}
