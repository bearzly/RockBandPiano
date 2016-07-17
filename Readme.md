# Rock Band 3 Piano Synth for Windows

## About

This project is a simple way to use a Rock Band 3 piano keyboard as a musical instrument when connected to 
a Windows PC, without the MIDI adapter. It is not a highly sophisticated synthesizer (this was my first time 
experimenting with audio code), but instead is intended to mostly serve as documentation of how the piano 
keyboard maps to an XInput device.

## Requirements

This is designed to work with an Xbox 360 piano controller for Rock Band 3. You will also need a USB wireless adapater 
for Xbox controllers in order to connect it to your computer.

This code uses RTAudio (included) as the interface to the Windows sound APIs

Audio synthesis is done using Tonic https://github.com/TonicAudio/Tonic

## Piano Controller

The piano controller was released along side Rock Band 3 and has the following inputs:
* 25 keys (2 full octaves)
* 4 Xbox face buttons (A/B/X/Y)
* 1 D-pad
* Start/Back buttons
* Xbox button
* "Overdrive" button
* Slide bar for modulating the sound of notes

When mapping this to XInput, this is how the inputs are encoded:
* Standard buttons and the D-pad map to the normal XInput wButtons bitmask
* The keys are encoded as a bitmask into the various analog fields (starting from the lowest note)
  * The bottom 8 notes are the bits of bLeftTrigger
  * The next 8 notes are the bits of bRightTrigger
  * The next 8 notes are the botom 8 bits of sThumbLX
  * The final note (highest C) is bit 15 of sThumbLX
* The overdrive button is bit 7 of sThumbRY
* The remaining bits of sThumbLX, sThumbLY, sThumbRx, and sThumbRY are used for encoding the intensity
of note presses. Each intensity gets 7 bits, and multiple intensities are recorded when chords are played.
* Touching the slide bar increases the XInput packet count, but does not map to any change in the XInput controller
state otherwise. The game may interpret a new packet number with no state change as an indication that there is a "whammy" going on

