#pragma once
#include "RtMidi.h"
#include <sys/utime.h>
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <atlbase.h>
#include <windows.h>
#include <string.h>
#include <tchar.h>
#include <conio.h>
#include <functional>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <thread>
#include <algorithm>
#include <future>

#define BUFSIZE 8192
#define NUMBER_OF_SCALE_TYPES 24

enum playStartType { immediate, nextBeat, nextBar };
enum gridMode { session, normal = 0, drums, keys, user };

// Play a note through a midi device
// portI:   index of the midi device in the midiout array
// chan:    desired midi channel
// note:    desired note value
// vel:     note velocity
// len:     note length in milliseconds
void playNoteGlobal(int portI, int chan, int note, int vel, int len);
int calculateNoteLength(int noteLengthIndex_x, int noteLengthIndex_y);
void gridMidiInCB(double, std::vector< unsigned char >*, void*);
void externKeyboardMidiInputCB(double, std::vector< unsigned char >*, void*);
void setLPtoProgrammerMode();
void sendWelcomeMessage();
void sendScrollTextToLP(std::string&, uint8_t, uint8_t, uint8_t, uint8_t);
time_t dateNowMillis();
time_t dateNowMicros();
int sendColors();
void copyCurrentPatternGridEnabledToGridColor();
void copyDrumModeColorsToGridColor();
void copyKeysModeColorsToGridColor();
void copyUserModeColorsToGridColor();
void clearGrid();
void drawGridNoteOpts();
bool updateMidiDevices();
DWORD WINAPI helperThreadFn(LPVOID);
DWORD WINAPI InstanceThread(LPVOID);
VOID ProcessPipeMessage(LPTSTR, LPTSTR, LPDWORD);
bool saveProject(std::string);
bool loadProject(std::string);
bool saveSettings();
bool loadSettings();
void sendMidiStartStop(bool);
void quitGridMidi();