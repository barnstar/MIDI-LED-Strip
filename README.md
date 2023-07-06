# MIDI LED Strip

A simple Midi lib to read events, and drive a WS2811 LED strip (via the included AdaFruit drivers).

As midi events are generated, the corresponding led will light up with a random hue with the intensity based on the velocity.  The LED strip will light up with with a "splash" effect, with the colour bleeding over to the surrounding leds.

The colour calculations here are just capable of running fairly well on an aruduino nano but pressing all 88 keys simultaneously or doing glissandos will choke things a bit.

The math is all done for you so you can overlay your strip over your keybaord and the spacing should exactly match the key spacing of a standard sized piano layout.
