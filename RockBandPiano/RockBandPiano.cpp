/* Copyright (c) 2016 Benjamin Gwin
 *
 * This file is licensed under the terms of the GPLv3.
 * See https://www.gnu.org/licenses/gpl-3.0.en.html for details.
 */

#include "stdafx.h"

#include "Tonic.h"
#include "RtAudio.h"

#include <windows.h>
#include <Xinput.h>

#include <iostream>
#include <vector>

using namespace Tonic;

static constexpr float BASE_NOTE = 65.41f; // Frequency of a C2
static constexpr float NOTE_RATIO = 1.059463094359f; // Ratio of frequencies for semitones
static constexpr int NUM_OCTAVES = 5; // Number of octaves to generate notes for
static constexpr int NOTES_PER_OCTAVE = 12; // Number of notes per octave
static constexpr int NUM_CHANNELS = 2; // Number of audio channels to use

static Synth synth;

// Encapsulates a specific note and its current state
class Note
{
	float freq;
	ControlValue volume;
	ControlTrigger trigger;
	Generator gen;
	bool on;

public:
	Note(float freq):
		freq(freq),
		volume(1.0), 
		on(false)
	{
		// Defines the overall sound envelope to avoid harsh edges/clicks
		ADSR env = ADSR()
		.attack(0.01f)
		.decay(0.3f)
		.sustain(0.8f)
		.release(0.05f)
		.doesSustain(true)
		.trigger(trigger);

		LPF12 filter = LPF12().cutoff(800);
		gen = env * volume.smoothed() * TriangleWave().freq(freq);
	}

	void enable()
	{ 
		if (!on)
			trigger.trigger(1.0); 
		on = true;
	}
	void disable()
	{
		if (on)
			trigger.trigger(0);
		on = false;
	}

	void setVolume(float vol)
	{
		volume = vol;
	}

	Generator getGen()
	{
		return gen;
	}
};

// Keeps track of the current octave etc. and polls for new XInput events
class KeyboardState
{
	DWORD lastPacket;
	unsigned octave;
	std::vector<Note> notes;
public:
	KeyboardState():
		lastPacket(0),
		octave(2),
		notes(notes)
	{
		float freq = BASE_NOTE;
		for (int i = 0; i < NUM_OCTAVES * NOTES_PER_OCTAVE + 1; ++i)
		{
			notes.push_back(Note(freq));
			freq *= NOTE_RATIO;
		}
	}

	Generator getGenerator()
	{
		Generator gen;
		for (auto& note : notes)
		{
			gen = gen + note.getGen();
		}
		return gen;
	}

	void processXInput()
	{
		XINPUT_STATE state;
		DWORD ret = XInputGetState(0, &state);
		if (ret != ERROR_SUCCESS)
			throw std::exception("xinput error", ret);

		if (state.dwPacketNumber == lastPacket)
			return;

		lastPacket = state.dwPacketNumber;

		if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) && octave < NUM_OCTAVES - 1)
			++octave;
		if ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) && octave > 0)
			--octave;

		std::cout << std::hex << state.Gamepad.sThumbRY << std::endl;

		// The state of the 25 buttons is encoded as a bit mask spread out
		// across the various analog inputs. This aggregates them into one
		// continuous int with bit 0 = highest note and bit 24 = lowest
		unsigned int noteMask = (state.Gamepad.bLeftTrigger << 17) |        // 8 notes from left trigger
			                    (state.Gamepad.bRightTrigger << 9) |        // 8 notes from right trigger
			                    ((state.Gamepad.sThumbLX & 0xff) << 1) |    // 8 notes from bottom byte of LX axis
			                    ((state.Gamepad.sThumbLX & 0x8000) >> 15);  // 1 note from bit 15 of LX axis
		
		// TODO: This is not fully researched. Volumes for multiple notes in a chord
		// are encoded in the remaining bits
		float volume = ((state.Gamepad.sThumbLX & 0x7f00) >> 8) / (float)0x7f;

		for (size_t i = 0; i < notes.size(); ++i)
		{
			notes[i].setVolume(volume);
			unsigned base = octave * NOTES_PER_OCTAVE;
			unsigned  top = base + NOTES_PER_OCTAVE * 2;
			if (i < base || i > top)
			{
				notes[i].disable();
				continue;
			}
			if (noteMask & (1 << (24 - (i - base))))
				notes[i].enable();
			else
				notes[i].disable();
		}
	}
};

// Called by RTAudio to get more audio data
int renderCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
				   double streamTime, RtAudioStreamStatus status, void *userData)
{
    synth.fillBufferOfFloats((float*)outputBuffer, nBufferFrames, NUM_CHANNELS);
    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    // Configure RtAudio
    RtAudio dac;
	if (dac.getDeviceCount() < 1) {
		std::cout << "\nNo audio devices found!\n";
		exit(1);
	}
    RtAudio::StreamParameters rtParams;
    rtParams.deviceId = dac.getDefaultOutputDevice();
    rtParams.nChannels = NUM_CHANNELS;
    unsigned int sampleRate = 44100;
    unsigned int bufferFrames = 512;
    
    Tonic::setSampleRate(sampleRate);
    
	KeyboardState keyboard;

    synth.setOutputGen(keyboard.getGenerator());
    
	// Open the audio stream and polling for events
    try
	{
        dac.openStream(&rtParams, NULL, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &renderCallback, NULL, NULL);
        dac.startStream();

		while (true)
		{
			keyboard.processXInput();
			Sleep(10);
		}
        
        dac.stopStream();
    }
    catch (RtError& e)
	{
        std::cout << "RT audio error:" << e.getMessage() << std::endl;
    }
	catch (std::exception& e)
	{
		std::cout << "Failed with:" << e.what() << std::endl;
	}

	dac.stopStream();
    
    return 0;
}

