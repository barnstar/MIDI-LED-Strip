#include "Adafruit_NeoPixel.h"
#include "MIDI.h"

#define LED_CONTROL_PIN 13                //The LED control pin
#define LED_COUNT 60                      //The number of LEDS in your strip.
#define LEDS_PER_PITCH (LED_COUNT / 72.0) //A typical 60 LED strip covers 72 keys
#define LED_START_PITCH 24                //Pitches below this will be ignored

//Arduino needs types defined up front
struct Pixel;
struct PixelFilter;

typedef struct Pixel
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t w = 0;

    void fade(float val)
    {
        r *= val;
        g *= val;
        b *= val;
        w *= val;
    }

    uint32_t pack()
    {
        return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    void combine(Pixel *p, float ratio)
    {
        r = min(uint8_t(r + (p->r * ratio)), 255);
        g = min(uint8_t(g + (p->g * ratio)), 255);
        b = min(uint8_t(b + (p->b * ratio)), 255);
    }

    void setRGB(uint8_t r, uint8_t g, uint8_t b)
    {
        this->r = r;
        this->g = g;
        this->b = b;
    }

    void setRGB(uint8_t *c)
    {
        this->r = (*c);
        this->g = *(c + 1);
        this->b = *(c + 2);
    }

    void copy(Pixel *p)
    {
        this->r = p->r;
        this->g = p->g;
        this->b = p->b;
    }

} Pixel;

typedef struct PixelFilter
{
    const short len;
    float filter[3];
} PixelFilter;

uint8_t colorIndex = 0;
#define COLOR_COUNT 6

/* Adds a random value to a random pixel in the given buffer */
Pixel dripVal;
Pixel *drip(Pixel *p, double intensity)
{
    const char colors[COLOR_COUNT][3] = {{255, 0, 0},
                                         {0, 255, 0},
                                         {0, 0, 255},
                                         {255, 255, 0},
                                         {0, 255, 255},
                                         {255, 0, 255}};

    colorIndex++;
    if (colorIndex == COLOR_COUNT)
        colorIndex = 0;
    dripVal.setRGB(&(colors[colorIndex][0]));
    dripVal.fade(intensity);
    p->combine(&dripVal, 1.0);
    return &dripVal;
}

/* Runs a low pass filter over backBuffer and stores the results in renderBuffer */
void blur(Pixel *renderBuffer, Pixel *backBuffer, size_t size, PixelFilter *filter)
{
    uint16_t halfLen = (filter->len) / 2;
    for (uint16_t i = 0; i < size; i++)
    {
        Pixel *dst = renderBuffer + i;
        Pixel val;
        for (uint16_t f = 0; f < filter->len; f++)
        {
            int16_t pos = i + (f - halfLen);
            if (pos < 0 || pos >= size)
                continue;
            Pixel *src = backBuffer + pos;
            val.combine(src, filter->filter[f]);
        }
        *dst = val;
    }
}

/* Renders the given buffer to the LED strip */
void render(Pixel *buffer, size_t size, Adafruit_NeoPixel *strip)
{
    for (uint16_t i = 0; i < size; i++)
    {
        Pixel *p = buffer + i;
        strip->setPixelColor(i, p->pack());
    }
    strip->show();
}

PixelFilter blur1 = {3, {0.1, .65, 0.1}};

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

/********************************************************************
 * Arduino Runloop
 * ******************************************************************/

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_CONTROL_PIN, NEO_GRB + NEO_KHZ800);

Pixel activeNotes[LED_COUNT];

#define PIXEL_OUT_OF_RANGE -1

MIDIInput midiInput = MIDIInput();

void setup()
{
    midiInput.start();
    strip.begin();
    strip.show();
}

void loop()
{
    MidiStatusMessage *statusMsg = &(midiInput.statusMsg);

    bool didRead = midiInput.readNextPendingEvent();
    //Read a note-on and the velocity is not zero
    if (didRead)
    {
        if ((statusMsg->status == MIDI_NOTE_ON) && (statusMsg->velocity() != 0))
        {
            //data[0] for NOTE_ON is the pitch.  data[1] is the velocity
            uint16_t position = pitchToPixelPosition(statusMsg->pitch());
            if (position != PIXEL_OUT_OF_RANGE)
            {
                Pixel *p = backBuffer + position;
                double velocity = statusMsg->velocity();
                //Compress the the intensity a bit
                double intensity = (0.5 + (velocity / 127.0)) / 1.5;
                Pixel *dripVal = drip(p, intensity);

                //The active notes array is use to control the sustain duration
                //The constants are all chosen to keep a nice balance between
                //busy and not too busy.  The values are all dependent on the
                //loop speed.  Adjust to taste.

                activeNotes[position].copy(dripVal);
                activeNotes[position].fade(0.4); //Magic!
            }
        }
        else if ((statusMsg->status == MIDI_NOTE_OFF) ||
                 ((statusMsg->status == MIDI_NOTE_ON) && statusMsg->velocity() == 0))
        {
            uint16_t position = pitchToPixelPosition(statusMsg->pitch());
            activeNotes[position].setRGB(0, 0, 0);
        }
        //Done with this status message
        statusMsg->reset()
    }

    for (int i = 0; i < LED_COUNT; i++)
    {
        double fadeVal = midiInput->sustainOn ? 0.99 : 0.97;
        activeNotes[i].fade(fadeVal); //More magic.  This is quite sensitive.
        Pixel *p = backBuffer + i;
        p->combine(activeNotes + i, 1.0);
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
    if (offsetPitch < 0)
    {
        return PIXEL_OUT_OF_RANGE;
    }

    double position = (double)offsetPitch * LEDS_PER_PITCH;
    if (position >= LED_COUNT)
    {
        return PIXEL_OUT_OF_RANGE;
    }

    return (uint16_t)position;
}
