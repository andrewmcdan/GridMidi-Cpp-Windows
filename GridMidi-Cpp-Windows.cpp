// cspell:disable

#include <functional>
#include <windows.h>
#include <string.h>
#include <tchar.h>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <sys/utime.h>
#include <ctime>
#include <thread>
#include <conio.h>
#include <algorithm>
#include <future>
#include "RtMidi.h"
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <atlbase.h>

#define DEBUG_EN
#define BUFSIZE 8192

const int DEFAULT_xSize = 12;
const int DEFAULT_ySize = 8;
const int DEFAULT_maxXsize = 64;
const int DEFAULT_maxYsize = 64;
const int DEFAULT_maxXstepSize = 384;
const int DEFAULT_maxYstepSize = 384;

// these constants get OR'ed with the channel when creating MIDI messages.
const unsigned char noteOnMessage = 0b10010000;
const unsigned char noteOffMessage = 0x80;

const unsigned char PLAYSTART_TYPE_IMMEDIATE = 1;
const unsigned char PLAYSTART_TYPE_NEXTBEAT = 2;
const unsigned char PLAYSTART_TYPE_NEXTBAR = 3;

// Play a note through a midi device
// portI:   index of the midi device in the midiout array
// chan:    desired midi channel
// note:    desired note value
// vel:     note velocity
// len:     note length in milliseconds
void playNoteGlobal(int portI, int chan, int note, int vel, int len);
int calculateNoteLength(int noteLengthIndex_x, int noteLengthIndex_y);

// program state struct for keeping track of program state
struct {
    bool midiDevsInit = false;
    bool launchPadInit = false;
}programState;

// settings struct for globally used settings values.
const struct {
    int maxXsize = DEFAULT_maxXsize;
    int maxYsize = DEFAULT_maxYsize;
    bool welcomeMessageEnabled = false;
} settings;

// struct for holding the data for each note on the grid
struct gridElement_t {
    uint8_t enabled = false;
    int note = 0;
    int velocity = 0;
    int noteLength_ms = 0;
    int noteLengthFractional_x = 0; 
    int noteLengthFractional_y = 0;
    bool noteLengthUseFractional = true;
    bool playing = false;
};

// midi output port info
struct outPort_t {
    int portIndex;
    int channel;
};

struct color_t {
    int r;
    int g;
    int b;
};

struct playingNote_t{
    int note;
    int portIndex;
    int channel;
};

class gridPattern {
public:
    // Set the callback functions for colring the play button
    void setCallBacks(std::function<void()>flashPlayCB, std::function<void()>playBtnOnCB, std::function<void()>playBtnOffCB) {
        playButtonOn = playBtnOnCB;
        playButtonOff = playBtnOffCB;
        flashPlayButton = flashPlayCB;
    }
    // gridPattern constructor
    // Generate grid with gridElement's and set default values.
    gridPattern() {
        outputPort.portIndex = 1;
        outputPort.channel = 0;
        color.r = 127;
        color.g = 0;
        color.b = 127;
        for (int i = 0; i < settings.maxXsize; i++) {
            grid.push_back(std::vector<gridElement_t>());
            for (int u = 0; u < settings.maxYsize; u++) {
                grid[i].push_back(gridElement_t());
                grid[i][u].enabled = 0;
                grid[i][u].note = 60;
                grid[i][u].velocity = 100;
                grid[i][u].noteLength_ms = 500;
                grid[i][u].noteLengthFractional_x = 3;
                grid[i][u].noteLengthFractional_y = 0;
                grid[i][u].playing = false;
            }
        }
    }
    bool toggleButton(int x, int y) {
        if ((x + this->xViewIndex) < this->xSize && (y + this->yViewIndex) < this->ySize) {
            if (this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled == 1) {
                this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled = 0;
            }
            else {
                this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled = 1;
            }
            this->lastSelected_X = x + this->xViewIndex;
            this->lastSelected_Y = y + this->yViewIndex;
            return true;
        }
        return false;
    }

    // increase the X size of the grid
    void increaseX(int amnt = 1) {
        if (this->xSize + amnt < settings.maxXsize) {
            this->xSize += amnt;
        }
    }

    // decrease the X size of the grid
    void decreaseX(int amnt = 1) {
        if (this->xSize - amnt > 0) {
            this->xSize -= amnt;
        }
    }
    // increase the Y size of the grid
    void increaseY(int amnt = 1) {
        if (this->ySize + amnt < settings.maxYsize) {
            this->ySize += amnt;
        }
    }
    // decrease the y size of the grid
    void decreaseY(int amnt = 1) {
        if (this->ySize - amnt > 0) {
            this->ySize -= amnt;
        }
    }
    // shifts view to the right by one
    void increaseXView() {
        // check bounds
        if (this->xViewIndex + 8 < this->xSize) {
            this->xViewIndex++;
        }
    }
    // shifts view to the ledt by one
    void decreaseXView() {
        if (this->xViewIndex > 0) {
            this->xViewIndex--;
        }
    }
    // shifts view up by one
    void increaseYView() {
        if (this->yViewIndex + 8 < this->ySize) {
            this->yViewIndex++;
        }
    }
    // shift view down by one
    void decreaseYView() {
        if (this->yViewIndex > 0) {
            this->yViewIndex--;
        }
    }
    // sets the velocity of the note at X,Y
    void setVelocity(int x, int y, int val = 0) {
        // checks bounds
        if (val < 128) {
            this->grid[x][y].velocity = val;
        }
        else {
            this->grid[x][y].velocity = 127;
        }
    }
    void setVelocity(int x, int y, int val, bool offset) {
        if (offset) {
            this->setVelocity(x, y, val);
        }
        else {
            this->setVelocity(x + this->xViewIndex, y + this->yViewIndex, val);
        }
    }
    int getVelocity(int x, int y) {
        return this->grid[x][y].velocity;
    }
    // sets note value of note at X,Y
    void setNote(int x, int y, int val = 0) {
        if (val < 128) {
            this->grid[x][y].note = val;
        }
        else {
            this->grid[x][y].note = 127;
        }
    }
    void setNote(int x, int y, int val, bool offset) {
        if (offset) {
            this->setNote(x, y, val);
        }
        else {
            this->setNote(x + this->xViewIndex, y + this->yViewIndex, val);
        }
    }

    int getNote(int x, int y) {
        return this->grid[x][y].note;
    }
    /// <summary>
    /// Set note length using milliseconds
    /// </summary>
    /// <param name="x">grid x value</param>
    /// <param name="y">grid y value</param>
    /// <param name="val">length in ms</param>
    void setNoteLength(int x, int y, int val) {
        if (val < 15000) {
            this->grid[x][y].noteLength_ms = val;
            this->grid[x][y].noteLengthUseFractional = false;
        }
    }
    void setNoteLength(int x, int y, int val, bool offset) {
        if (offset) {
            this->setNoteLength(x, y, val);
        }
        else {
            this->setNoteLength(x + this->xViewIndex, y + this->yViewIndex, val);
        }
    }
    /// <summary>
    /// Set note length using calculateNoteLenght()
    /// </summary>
    /// <param name="x">Grid x value</param>
    /// <param name="y">Grid y value</param>
    /// <param name="val_x">Determines if note is made of half notes, quarternotes, eighths, etc.</param>
    /// <param name="val_y">Determines how many of the above the note is made of.</param>
    void setNoteLength(int x, int y, int val_x, int val_y) {
        if (val_x < 9 && val_y < 9) {
            this->grid[x][y].noteLengthFractional_x = val_x;
            this->grid[x][y].noteLengthFractional_y = val_y;
            this->grid[x][y].noteLengthUseFractional = true;
        }
    }
    void setNoteLength(int x, int y, int val_x, int val_y, bool offset) {
        if (offset) {
            this->setNoteLength(x, y, val_x, val_y);
        }
        else {
            this->setNoteLength(x + this->xViewIndex, y + this->yViewIndex, val_x, val_y);
        }
    }
    int getNoteLength(int x, int y) {
        if (this->grid[x][y].noteLengthUseFractional) {
            return calculateNoteLength(this->grid[x][y].noteLengthFractional_x, this->grid[x][y].noteLengthFractional_y);
        }
        else
            return this->grid[x][y].noteLength_ms;
    }
    void getNoteLengthFractional(int x, int y, int* coords) {
        coords[0] = this->grid[x][y].noteLengthFractional_x;
        coords[1] = this->grid[x][y].noteLengthFractional_y;
    }

    bool getIsUsingFractionalNoteLength(int x, int y, bool offset = false) {
        if (offset) {
            return this->grid[x][y].noteLengthUseFractional;
        }
        else {
            return this->grid[x + this->xViewIndex][y + this->yViewIndex].noteLengthUseFractional;
        }
        
    }

    // return true/false bassed on note enabled or not
    // if offset == 1, take into account view shift
    bool getNoteEnabled(int x, int y, int offset) {
        if (offset == 1) {
            return this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled == 1;
        }
        else {
            return this->grid[x][y].enabled == 1;
        }
    }

    //return whether or not the particular note was already started playing.
    bool getNotePlaying(int x, int y) {
        return this->grid[x][y].playing;
    }
    
    void setNotePlaying(int x, int y, bool playingBool) {
        this->grid[x][y].playing = playingBool;
    }

    void playNote(int x, int y) {
        // set note playing state
        this->setNotePlaying(x, y, true);
        // play the note
        playNoteGlobal(this->outputPort.portIndex, this->outputPort.channel, this->getNote(x, y), this->getVelocity(x, y), this->getNoteLength(x, y));
        // using std::async, set note playing state to false after the note length time has expired.
        // the std::future generated by the std::async call has to be stored at least for as long as it takes for the note to expire. 
        playNoteResetFuturesArray[playNoteFuturesArrayIndex] = std::async(std::launch::async, [=](){
            Sleep(this->getNoteLength(x,y));
            this->setNotePlaying(x, y, false);
        });
        playNoteFuturesArrayIndex++;
        if (playNoteFuturesArrayIndex >= 1024)playNoteFuturesArrayIndex = 0;
    }

    void playStepX() {
        this->currentXstep++;
        if (this->currentXstep >= this->xSize)this->currentXstep = 0;
        switch (this->mode) {
        case patternModes::XY_OR:
            for (uint8_t i = 0; i < this->grid[this->currentXstep].size(); i++) {
                if (this->getNoteEnabled(this->currentXstep, i, 0)) {
                    this->playNote(this->currentXstep, i);
                }
            }
            break;
        default:
            break;
        }
    }

    void playStepY() {
        this->currentYstep++;
        if (this->currentYstep >= this->ySize)this->currentYstep = 0;

        switch (this->mode) {
        case patternModes::XY_OR:
            for (uint8_t i = 0; i < grid.size(); i++) {
                if (this->getNoteEnabled(i, this->currentYstep, 0) && !this->getNotePlaying(i, this->currentYstep)) {
                    this->playNote(i, this->currentYstep);
                }
            }
            break;
        case patternModes::XY_AND:
            if (this->getNoteEnabled(this->currentXstep, this->currentYstep, 0) && !this->getNotePlaying(this->currentXstep, this->currentYstep)) {
                this->playNote(this->currentXstep, this->currentYstep);
            }
            break;
        default:
            break;
        }
        
    }

    void tick() {
        if (this->tickCount % 24 == 0) {
            this->flashPlayButton();
        }
        switch (this->mode) {
        case patternModes::XY_OR:
        case patternModes::XY_AND:
            if (this->tickCount % this->stepSizeX == 0) {
                this->playStepX();
            }

            if (this->tickCount % this->stepSizeY == 0) {
                this->playStepY();
            }
            break;
        case patternModes::X_SEQ:
            if (this->tickCount % this->stepSizeX == 0) {
                this->playStepX();
            }
            break;
        case patternModes::Y_SEQ:
            if (this->tickCount % this->stepSizeY == 0) {
                this->playStepY();
            }
            break;
        case patternModes::STEP64:
            break;
        default:
            break;
        }
        this->tickCount++;
        if (this->tickCount >= this->tickResetVal)this->tickCount = 0;
    }

    void tickReset() {
        this->tickCount = 0;
        this->currentXstep = this->xSize;
        this->currentYstep = this->ySize;
        this->playButtonOn();
    }

    int getCurrentGridX() {
        //if (this->currentXstep - this->xViewIndex - 1 < 0)return this->xSize - 1;
        return this->currentXstep - this->xViewIndex ;
    }

    int getCurrentGridY() {
        //if (this->currentYstep - this->yViewIndex - 1 < 0)return this->ySize - 1;
        return this->currentYstep - this->yViewIndex ;
    }

    outPort_t getOutPort() {
        return this->outputPort;
    }

    void setOutPort(int portNum, int channel) {
        this->outputPort.channel = channel;
        this->outputPort.portIndex = portNum;
    }

    void turnOnPlayButton() {
        this->playButtonOn();
    }

    void setNumStepsAndSizes(uint16_t xNumSteps, uint16_t yNumSteps, uint16_t xStepSize, uint16_t yStepSize) {
        if (xNumSteps < DEFAULT_maxXsize && yNumSteps < DEFAULT_maxYsize && xStepSize < DEFAULT_maxXstepSize && yStepSize < DEFAULT_maxYstepSize) {
            this->xSize = xNumSteps;
            this->ySize = yNumSteps;
            this->stepSizeX = xStepSize;
            this->stepSizeY = yStepSize;
            if (!this->playing) {
                this->currentXstep = this->xSize;
                this->currentYstep = this->ySize;
            }
        }
    }

    int getMode() {
        return this->mode;
    }

    void setMode(int m) {
        this->mode = patternModes(m);
        //this->playing = false;
        this->tickReset();
    }

    std::function<void()>playButtonOn;
    std::function<void()>playButtonOff;
    std::function<void()>flashPlayButton;
    int xSize = DEFAULT_xSize;
    int ySize = DEFAULT_ySize;
    int currentXstep = xSize;
    int currentYstep = ySize;
    bool playing = false;
    int playStartType = PLAYSTART_TYPE_IMMEDIATE;
    uint8_t lastSelected_X = 0;
    uint8_t lastSelected_Y = 0;
    int stepSizeX = 24;
    int stepSizeY = 24;
    enum patternModes { XY_OR, XY_AND, X_SEQ, Y_SEQ, STEP64, X_then_Y };
private:
    std::vector<std::vector<gridElement_t>>grid;
    std::future<void>playNoteResetFuturesArray[1024];
    int playNoteFuturesArrayIndex = 0;
    int xViewIndex = 0;
    int yViewIndex = 0;
    uint32_t tickCount = 0;
    uint32_t tickResetVal = 2000000;
    outPort_t outputPort;
    color_t color;
    patternModes mode = XY_OR;
};

struct gS_t {
    gS_t() {
        // Buttons across top
        for (int i = 0; i < 8; i++) {
            otherColor.push_back(color_t());
            otherColor[i].r = 10;
            otherColor[i].g = 0;
            otherColor[i].b = 0;
        }
        // Buttons down the side
        for (int i = 8; i < 16; i++) {
            otherColor.push_back(color_t());
            otherColor[i].r = 0;
            otherColor[i].g = 0;
            otherColor[i].b = 0;
        }
        // Stop, Solo, Mute button
        otherColor[8].r = 10;
        otherColor[8].g = 0;
        otherColor[8].b = 0;

        for (int x = 0; x < 8; x++) {
            gridColor.push_back(std::vector<color_t>());
            for (int y = 0; y < 8; y++) {
                gridColor[x].push_back(color_t());
                gridColor[x][y].r = 0;
                gridColor[x][y].g = 0;
                gridColor[x][y].b = 0;
            }
        }

        logoColor.r = 127;
        logoColor.g = 127;
        logoColor.b = 127;

        for (int i = 0; i < 7; i++) {
            patterns.push_back(gridPattern());
            patterns[i].setCallBacks(
                [this, i] () {
                    if (this->currentSelectedPattern == i) {
                        otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                        otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 127 : 0;
                        otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 127 : 0;
                    }
                    else {
                        otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                        otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 127 : 0;
                        otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 0 : 0;
                    }
                }, 
                [this, i] () {
                    otherColor[15 - i].r = 127;
                    otherColor[15 - i].g = 127;
                    otherColor[15 - i].b = 127;
                }, 
                [this, i] () {
                    otherColor[15 - i].r = 0;
                    otherColor[15 - i].g = 0;
                    otherColor[15 - i].b = 0;
                });
        }

        patterns[0].playButtonOn();
    }
    std::vector<color_t> otherColor;
    std::vector<std::vector<color_t>> gridColor;
    color_t logoColor;
    int bpm = 110;
    std::string gridMode = "normal";
    int currentSelectedPattern = 0;
    int numberOfPlayingPatterns = 0;
    std::vector<gridPattern>patterns;
    bool playing = false;
};

void gridMidiInCB(double, std::vector< unsigned char >* message, void* /*userData*/);
void setLPtoProgrammerMode(RtMidiOut*);
void sendWelcomeMessage(RtMidiOut*);
void sendScrollTextToLP(RtMidiOut*, std::string, uint8_t, uint8_t, uint8_t, uint8_t);
time_t dateNowMillis();
time_t dateNowMicros();
time_t sendColors(time_t lastSentTime, RtMidiOut* rtmidiout, bool overrideThrottle);
void copyCurrentPatternGridEnabledToGridColor();
void clearGrid();
void drawGridNoteOpts();
void updateMidiDevices();
DWORD WINAPI helperThreadFn(LPVOID);
DWORD WINAPI InstanceThread(LPVOID);
VOID GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);

gS_t gridState;

RtMidiIn* gridMidiIn = 0; // RTmidi object for LPminiMK3
RtMidiIn* midiIn[128]; // RTmidi objects for all other midi devs
int gridMidiInDevIndex = -1; // indicates which element in midiIn[] will be uninitialized due to it correlating to LPminiMK3
uint16_t numMidiInDevs = 0;
std::string midiInPortNames[128];
int keyboardInputDevIndex = -1;
bool midiInputDevicesEnabled[128];

RtMidiOut* gridMidiOut = 0; // RtMidi object for LPminiMK3
RtMidiOut* midiOut[128]; // RtMidi objects for all other midi devs
int gridMidiOutDevIndex = -1;
uint16_t numMidiOutDevs = 0;
std::string midiOutPortNames[128];
bool midiOutputDevicesEnabled[128];
bool midiOutputDevicesClockEn[128];
int drumsModeOutputDevIndex = -1;
int keysModeOutputDevIndex = -1;

playingNote_t currentPlayingNotes[1024];
time_t playingNotesExpireTimes[1024];
int currentPlayingNotesIndex = 0;

int main(int argc, char *argv[])
{
    HANDLE helperThread = NULL;
    DWORD  dwThreadId = 0;
    printf("Creating a helper thread.\n");

    // Create a thread for this client. 
    helperThread = CreateThread(
        NULL,              // no security attribute 
        0,                 // default stack size 
        helperThreadFn,    // thread proc
        NULL,    // thread parameter 
        0,                 // not suspended 
        &dwThreadId);      // returns thread ID 

    if (helperThread == NULL)
    {
        _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
    }
    //else CloseHandle(helperThread);


    for (int i = 0; i < 128; i++) {
        midiOutputDevicesEnabled[i] = false;
        midiOutputDevicesClockEn[i] = false;
        midiInputDevicesEnabled[i] = false;
    }

    for (int i = 0; i < 1024; i++) {
        playingNotesExpireTimes[i] = 0;
    }

    try {
        // init all the placeholders for midi devices
        for (int i = 0; i < 128; i++) {
            midiIn[i] = new RtMidiIn();
        }
        // init gridmidiIn
        gridMidiIn = new RtMidiIn();
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
    try {
        // init all the placeholders for midi devices
        for (int i = 0; i < 128; i++) {
            midiOut[i] = new RtMidiOut();
        }

        // init gridmidiout
        gridMidiOut = new RtMidiOut();
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
    updateMidiDevices();

    programState.midiDevsInit = true;

    setLPtoProgrammerMode(gridMidiOut);
    sendWelcomeMessage(gridMidiOut);
    copyCurrentPatternGridEnabledToGridColor();

    time_t tempTime = dateNowMicros();
    time_t timerTime = dateNowMicros();
    time_t interval_1ms = 0;
    time_t interval_30ms = 0;
    time_t interval_2s = 0;
    time_t tickTimer_1 = dateNowMillis();
    int logoCycleCount = 0;

    while (1) {
        tempTime = dateNowMicros();
        time_t timeDiff = tempTime - timerTime;
        timerTime = dateNowMicros();
        interval_1ms += timeDiff;
        interval_30ms += timeDiff;
        interval_2s += timeDiff;

        if (interval_1ms > 1000) {
            interval_1ms = 0;
            time_t tickTimer_2 = dateNowMillis();
            time_t tickTime = ((60 / (double(gridState.bpm) / 4)) / 96) * 1000;
            if (tickTimer_2 - tickTimer_1 > tickTime) {
                tickTimer_1 = dateNowMillis();
                if (gridState.playing) {
                    for (int i = 0; i < 128; i++) {
                        if (midiOutputDevicesClockEn[i]) {
                            std::vector<unsigned char>message;
                            message.push_back(248);
                            midiOut[i]->sendMessage(&message);
                        }
                    }
                    for (uint8_t i = 0; i < gridState.patterns.size(); i++) {
                        if (gridState.patterns[i].playing) {
                            gridState.patterns[i].tick();
                        }
                    }
                }
            }

            time_t tempNow = dateNowMillis();
            for (int i = 0; i < 1024; i++) {
                if (playingNotesExpireTimes[i] < tempNow && playingNotesExpireTimes[i] > 0) {
                    std::vector<unsigned char>message;
                    message.push_back(noteOffMessage | currentPlayingNotes[i].channel);
                    message.push_back(currentPlayingNotes[i].note);
                    message.push_back(0);
                    midiOut[currentPlayingNotes[i].portIndex]->sendMessage(&message);
                    playingNotesExpireTimes[i] = 0;
                }
            }
        }

        if (interval_30ms > 30000) {
            interval_30ms = 0;
            copyCurrentPatternGridEnabledToGridColor();
            sendColors(0, gridMidiOut, true);
            double frequency = 0.1;
            gridState.logoColor.r = int(sin(frequency * logoCycleCount + 0) * 63 + 64) | 0;
            gridState.logoColor.g = int(sin(frequency * logoCycleCount + 2) * 63 + 64) | 0;
            gridState.logoColor.b = int(sin(frequency * logoCycleCount + 4) * 63 + 64) | 0;
            logoCycleCount++;
            if (logoCycleCount >= 64)logoCycleCount = 0;
        }

        if (interval_2s > 2000000) {
            interval_2s = 0;
        }
        
        if (_kbhit()) {
#ifdef DEBUG_EN
            break;
#endif // DEBUG_EN
            std::string s = "";
            std::cout << "Exit? (y/n): ";
            std::cin >> s;
            if(s=="y")break;
        }
        std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::microseconds(10));
    }

    for (int i = 0; i < 128; i++) {
        delete midiIn[i];
    }
    delete gridMidiIn;
    for (int i = 0; i < 128; i++) {
        delete midiOut[i];
    }
    delete gridMidiOut;
    return 0;
}

DWORD WINAPI helperThreadFn(LPVOID lpvParam) {
#ifdef DEBUG_EN
    printf("Helper thread started...\n");
#endif // DEBUG_EN
    BOOL   fConnected = FALSE;
    DWORD  dwThreadId = 0;
    HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\gridMidi");

    // The main loop creates an instance of the named pipe and 
    // then waits for a client to connect to it. When the client 
    // connects, a thread is created to handle communications 
    // with that client, and this loop is free to wait for the
    // next client connect request. It is an infinite loop.

    for (;;)
    {
#ifdef DEBUG_EN
        //_tprintf(TEXT("Pipe Server: Helper thread awaiting client connection on %s\n"), lpszPipename);
#endif // DEBUG_EN
        hPipe = CreateNamedPipe(
            lpszPipename,             // pipe name 
            PIPE_ACCESS_DUPLEX,       // read/write access 
            PIPE_TYPE_MESSAGE |       // message type pipe 
            PIPE_READMODE_MESSAGE |   // message-read mode 
            PIPE_WAIT,                // blocking mode 
            PIPE_UNLIMITED_INSTANCES, // max. instances  
            BUFSIZE,                  // output buffer size 
            BUFSIZE,                  // input buffer size 
            0,                        // client time-out 
            NULL);                    // default security attribute 

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            _tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
            return -1;
        }

        // Wait for the client to connect; if it succeeds, 
        // the function returns a nonzero value. If the function
        // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

        fConnected = ConnectNamedPipe(hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (fConnected)
        {
            //printf("Client connected, creating a processing thread.\n");

            // Create a thread for this client. 
            hThread = CreateThread(
                NULL,              // no security attribute 
                0,                 // default stack size 
                InstanceThread,    // thread proc
                (LPVOID)hPipe,    // thread parameter 
                0,                 // not suspended 
                &dwThreadId);      // returns thread ID 

            if (hThread == NULL)
            {
                _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
                return -1;
            }
            else CloseHandle(hThread);
        }
        else
            // The client could not connect, so close the pipe. 
            CloseHandle(hPipe);
    }

    //return 0;
    printf("HelperThreadFn exiting.\n");
    return 1;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
    HANDLE hHeap = GetProcessHeap();
    TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.

    if (lpvParam == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL value in lpvParam.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    if (pchRequest == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        return (DWORD)-1;
    }

    if (pchReply == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    // Print verbose messages. In production code, this should be for debugging only.
    //printf("InstanceThread created, receiving and processing messages.\n");

    // The thread's parameter is a handle to a pipe object instance. 

    hPipe = (HANDLE)lpvParam;

    // Loop until done reading
    while (1)
    {
        // Read client requests from the pipe. This simplistic code only allows messages
        // up to BUFSIZE characters in length.
        fSuccess = ReadFile(
            hPipe,        // handle to pipe 
            pchRequest,    // buffer to receive data 
            BUFSIZE * sizeof(TCHAR), // size of buffer 
            &cbBytesRead, // number of bytes read 
            NULL);        // not overlapped I/O 

        if (!fSuccess || cbBytesRead == 0)
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                //_tprintf(TEXT("InstanceThread: client disconnected.\n"));
            }
            else
            {
                _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
            }
            break;
        }

        // Process the incoming message.
        GetAnswerToRequest(pchRequest, pchReply, &cbReplyBytes);

        //printf("number of bytes: %i\n", cbReplyBytes);

        // Write the reply to the pipe. 
        fSuccess = WriteFile(
            hPipe,        // handle to pipe 
            pchReply,     // buffer to write from 
            cbReplyBytes, // number of bytes to write 
            &cbWritten,   // number of bytes written 
            NULL);        // not overlapped I/O 

        if (!fSuccess || cbReplyBytes != cbWritten)
        {
            _tprintf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
            break;
        }
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);

    //printf("InstanceThread exiting.\n");
    return 1;
}

VOID GetAnswerToRequest(LPTSTR pchRequest, LPTSTR pchReply, LPDWORD pchBytes)
{
#ifdef DEBUG_EN1
    time_t before = dateNowMicros();
#endif // DEBUG_EN
    // check that controller is ready
    if (!programState.midiDevsInit || !programState.launchPadInit) {
        *pchBytes = 0;
        pchReply[0] = 0;
        printf("Controller not ready (midiDevsInit).\n");
#ifdef DEBUG_EN1
        time_t after = dateNowMicros();
        std::cout << "time: " << after - before << std::endl;
#endif // DEBUG
        return;
    }

    _tprintf(TEXT("Client Request String:\"%s\"\n"), pchRequest);
    ///////////////////////////////////////////////////////////////
    // process IPC string
    std::string stdString_Request;
    std::string reply = "";
    std::wstring w = pchRequest;
    stdString_Request = std::string(w.begin(), w.end()); // magic here
    if (stdString_Request.substr(0,7) == "isReady") {
        reply = "ready";
        // Check the outgoing message to make sure it's not too long for the buffer.
        if (FAILED(StringCchCopy(pchReply, BUFSIZE, std::wstring(reply.begin(), reply.end()).c_str()))){
            *pchBytes = 0;
            pchReply[0] = 0;
            printf("StringCchCopy failed, no outgoing message.\n");
            return;
        }
        *pchBytes = (lstrlen(pchReply) + 1) * sizeof(TCHAR);
#ifdef DEBUG_EN1
        time_t after = dateNowMicros();
        std::cout << "time: " << after - before << std::endl;
#endif // DEBUG_EN
        return;
    }
    std::string command1 = stdString_Request.substr(0, 3); // first 3 characters are either req or dat for request or data
    std::string command2 = stdString_Request.substr(3, 16); // Next 8 characters are sub commands
    std::string command3 = "";
    if(stdString_Request.size() > 18)
        command3 = stdString_Request.substr(19);   // Remainder of string is formatted per command
    
    // "req": request for data
    if (command1 == "req") {
        // "numMidiDevs____": request number of midi devices
        if (command2 == "numMidiDevs_____") {
            reply = "numMidiInDevs:" + std::to_string(numMidiInDevs) + ";numMidiOutDevs:" + std::to_string(numMidiOutDevs);
        }
        // "midiInDevNames_": request names of midi In devices
        else if (command2 == "midiInDevNames__") {
            reply = "";
            for (int i = 0; i < numMidiInDevs; i++) {
                reply += midiInPortNames[i] + ";\n";
            }
        }
        // "midiOutDevNames": request names of midi out devices
        else if (command2 == "midiOutDevNames_") {
            reply = "";
            for (int i = 0; i < numMidiOutDevs; i++) {
                reply += midiOutPortNames[i] + ";\n";
            }
        }
        // "getBPM_________": get the current BPM
        else if (command2 == "getBPM__________") {
            reply = std::to_string(gridState.bpm);
        }
        // "getMOutDevClkEn": get state of clock output enabled for each midi device
        else if (command2 == "getMOutDevClkEn_") {
            reply = "";
            for (int i = 0; i < numMidiOutDevs; i++) {
                reply += (midiOutputDevicesClockEn[i] ? "1" : "0");
                reply += i == numMidiOutDevs - 1 ? "" : ":";
            }
        }
        // "getMidiDevOutEn":
        else if (command2 == "getMidiDevOutEn_") {
            reply = "";
            for (int i = 0; i < numMidiOutDevs; i++) {
                reply += (midiOutputDevicesEnabled[i] ? "1" : "0");
                reply += i == numMidiOutDevs - 1 ? "" : ":";
            }
        }
        else if (command2 == "getMidiDevInEn__") {
            reply = "";
            for (int i = 0; i < numMidiInDevs; i++) {
                reply += (midiInputDevicesEnabled[i] ? "1" : "0");
                reply += i == numMidiInDevs - 1 ? "" : ":";
            }
        }
        else if (command2 == "getKbMidiInDev__") {
            if (keyboardInputDevIndex != -1) {
                reply = midiInPortNames[keyboardInputDevIndex];
            }
            else {
                reply = "Disabled  ";
            }
        }
        else if (command2 == "getDrumsModeODev") {
            if (drumsModeOutputDevIndex != -1) {
                reply = midiOutPortNames[drumsModeOutputDevIndex];
            }
            else {
                reply = "Disabled  ";
            }
        }
        else if (command2 == "getKeysModeODev_") {
            if (keysModeOutputDevIndex != -1) {
                reply = midiOutPortNames[keysModeOutputDevIndex];
            }
            else {
                reply = "Disabled  ";
            }
        }
        else if (command2 == "getCurrentGridXY") {
            reply = "";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y);
        }
        else if (command2 == "getNoteData_____") {
            reply = "";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].getNote(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X, gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y));
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].getVelocity(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X, gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y));
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].getNoteLength(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X, gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y));
            reply += ":";
            int xy[2] = { 0,0 };
            gridState.patterns[gridState.currentSelectedPattern].getNoteLengthFractional(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X, gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y, xy);
            reply += std::to_string(xy[0]);
            reply += ":";
            reply += std::to_string(xy[1]);
            reply += ":";
            reply += (gridState.patterns[gridState.currentSelectedPattern].getIsUsingFractionalNoteLength(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X, gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y) ? "1" : "0");
        }
        else if (command2 == "PatternOptions__") {
            reply = "";
            reply += std::to_string(gridState.currentSelectedPattern);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].getMode());
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].xSize);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].ySize);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].stepSizeX);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].stepSizeY);
        }
        else if (command2 == "PatternOutPort__") {
            reply = "";
            reply += midiOutPortNames[gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex];
        }
    }
    else if (command1 == "dat") {
#ifdef DEBUG_EN
        std::cout << "command is data" << std::endl << "command2: " << command2 << std::endl;
#endif // DEBUG_EN
        reply = "Data command recieved.";
        if (command2 == "updateBPM_______") {
            int newBPM = std::stoi(command3);
            gridState.bpm = newBPM;
        }
        else if (command2 == "selectPattern___") {
            int patNum = std::stoi(command3);
            if (patNum < 8 && patNum >= 0) {
                for (int e = 0; e < 7; e++) {
                    if (!gridState.patterns[e].playing) {
                        gridState.patterns[e].playButtonOff();
                    }
                }
                gridState.currentSelectedPattern = patNum;
                gridState.patterns[patNum].playButtonOn();
            }
        }
        else if (command2 == "setKbInputDev___") {
            for (int i = 0; i < numMidiInDevs; i++) {
                if (midiInPortNames[i].find(command3) != std::string::npos) {
                    keyboardInputDevIndex = i;
                }
            }
        }
        else if (command2 == "setDrumOutputDev") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                    drumsModeOutputDevIndex = i;
                }
            }
        }
        else if (command2 == "setKeysOutputDev") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                    keysModeOutputDevIndex = i;
                }
            }
        }
        else if (command2 == "setMidiDevInEn__") {
            for (int i = 0; i < numMidiInDevs; i++) {
                if (midiInPortNames[i].find(command3.substr(0,command3.length() - 2)) != std::string::npos) {
                    midiInputDevicesEnabled[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
            updateMidiDevices();
        }
        else if (command2 == "setMidiOutDevEn_") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3.substr(0, command3.length() - 2)) != std::string::npos) {
                    midiOutputDevicesEnabled[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
            updateMidiDevices();
        }
        else if (command2 == "setMidiOutClkEn_") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3.substr(0, command3.length() - 2)) != std::string::npos) {
                    midiOutputDevicesClockEn[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
        }
        else if (command2 == "setNoteValue____") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNote(x, y, val, true);
        }
        else if (command2 == "setNoteVelocity_") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setVelocity(x,y,val,true);
        }
        else if (command2 == "setNoteLength_ms") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNoteLength(x,y,val,true);
        }
        else if (command2 == "setNoteLenFrctnl") {
            //std::cout << "command3: " << command3 << std::endl;
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int xf = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int yf = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNoteLength(x,y,xf,yf,true);
            
            //std::cout << "x: " << x << std::endl;
            //std::cout << "y: " << y << std::endl;
            //std::cout << "xf: " << xf << std::endl;
            //std::cout << "yf: " << yf << std::endl;
        }
        else if (command2 == "patternOptions__") {
            int chan = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int mode = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int xSteps = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int ySteps = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int xStepSize = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int yStepSize = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNumStepsAndSizes(xSteps, ySteps, xStepSize, yStepSize);
            if (chan < 16 && chan >= 0) {
                gridState.patterns[gridState.currentSelectedPattern].setOutPort(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, chan);
            }
            std::cout << "mode: " << mode << std::endl;
            gridState.patterns[gridState.currentSelectedPattern].setMode(mode);
        }
        else if (command2 == "patternMidiDev__") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                     gridState.patterns[gridState.currentSelectedPattern].setOutPort(i, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel);
                }
            }
        }
        else {
#ifdef DEBUG_EN
            std::cout << "data command not recognised" << std::endl;
#endif
        }
    }
    else {
#ifdef DEBUG_EN
        std::cout << "command was not valid" << std::endl;
#endif
    }
#ifdef DEBUG_EN1
    time_t after = dateNowMicros();
    std::cout << "time: " << after - before << std::endl;
#endif // DEBUG_EN
    ///////////////////////////////////////////////////////////////
    // Check the outgoing message to make sure it's not too long for the buffer.
    if (FAILED(StringCchCopy(pchReply, BUFSIZE, std::wstring(reply.begin(),reply.end()).c_str())))
    {
        *pchBytes = 0;
        pchReply[0] = 0;
        printf("StringCchCopy failed, no outgoing message.\n");
        return;
    }
    *pchBytes = (lstrlen(pchReply) + 1) * sizeof(TCHAR);
}

time_t gridButtonDownTime[64];
time_t gridButtonUpTime[64];
int gridButtonsPressed[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int gridButtonsPressedIterator = 0;
int lastPressedGridButton[2] = { 0, 0 };
int tempVelocity = 0;
int tempNote = 0;
int tempOctave = 5;
int tempnoteLength[2] = { 0, 0 };
time_t playButtonDownTime = dateNowMillis();
int lastPressedPlayButton[2] = { 0, 0 };

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/)
{
    /*int nBytes = message->size();
    for (int i = 0; i < nBytes; i++)
        std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
    if (nBytes > 0)
        std::cout << "stamp = " << deltatime << std::endl;
    */

    int gridY = (((int)message->at(1) / 10) | 0) - 1;
    int gridX = ((int)message->at(1) % 10) - 1;

    // if the first byte of the message was 176 and the 3rd was 127, then that was a button down
    // for any button not on the grid.
    if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
        playButtonDownTime = dateNowMillis();
        int patternNum = 0;
        switch ((int)message->at(1)) {
        case 91: {
            gridState.patterns[gridState.currentSelectedPattern].increaseYView();
            break;
        }
        case 92: {
            gridState.patterns[gridState.currentSelectedPattern].decreaseYView();
            break;
        }
        case 93: {
            gridState.patterns[gridState.currentSelectedPattern].decreaseXView();
            break;
        }
        case 94: {
            gridState.patterns[gridState.currentSelectedPattern].increaseXView();
            break;
        }
        case 89: {
            // top play button
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 89;
            patternNum = 0;
            break;
        }
        case 79: {
            // second play button
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 79;
            patternNum = 1;
            break;
        }
        case 69: {
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 69;
            patternNum = 2;
            break;
        }
        case 59: {
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 59;
            patternNum = 3;
            break;
        }
        case 49: {
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 49;
            patternNum = 4;
            break;
        }
        case 39: {
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 39;
            patternNum = 5;
            break;
        }
        case 29: {
            lastPressedPlayButton[1] = lastPressedPlayButton[0];
            lastPressedPlayButton[0] = 29;
            // bottom play button
            patternNum = 6;
            break;
        }
        case 19: {
            // stop, mute, solo button
            patternNum = 7;

            gridState.currentSelectedPattern = 0;
            // patternNum = 0;

            // stop palying each pattern, reset it, and reset number of playing patterns. This will
            // stop the global playing, tick, and clock output.
            for (uint8_t i = 0; i < gridState.patterns.size(); i++) {
                gridState.patterns[i].playing = false;
                gridState.patterns[i].tickReset();
            }
            gridState.numberOfPlayingPatterns = 0;
            break;
        }
        }

        // if the midi message is for one of the play buttons....
        if ((int)message->at(1) < 90) {
            if (patternNum != gridState.currentSelectedPattern) {
                gridState.currentSelectedPattern = patternNum;
                // turn off all the play buttons but the selected pattern
                for (int e = 0; e < 7; e++) {
                    if (!gridState.patterns[e].playing) {
                        gridState.patterns[e].playButtonOff();
                    }
                }
                // if the button that was pressed was anyhting but the stop button,
                // turn on the play button for the pattern selected
                // otherwise, turn on the first play button and select it.
                if (patternNum < 7) {
                    gridState.patterns[patternNum].playButtonOn();
                }
                else {
                    gridState.currentSelectedPattern = 0;
                    gridState.patterns[0].playButtonOn();
                }

            }
            else {
                // toggle playing state for pattern
                if (gridState.playing) {
                    switch (gridState.patterns[patternNum].playStartType) {
                    case PLAYSTART_TYPE_IMMEDIATE: {
                        gridState.patterns[patternNum].playing = !gridState.patterns[patternNum].playing;

                        // set overall play state to true
                        if (gridState.patterns[patternNum].playing) gridState.playing = true;
                        // if pattern was toggled off, reset tick and reduce number of playing patterns
                        if (!gridState.patterns[patternNum].playing) {
                            gridState.patterns[patternNum].tickReset();
                            gridState.numberOfPlayingPatterns--;
                        }
                        else {
                            // increase number of playing patterns
                            gridState.numberOfPlayingPatterns++;
                        }
                        break;
                    }
                    case PLAYSTART_TYPE_NEXTBEAT: {
                        //@TODO
                        break;
                    }
                    case PLAYSTART_TYPE_NEXTBAR: {
                        //@TODO
                        break;
                    }
                    }
                }
                else {


                    gridState.patterns[patternNum].playing = !gridState.patterns[patternNum].playing;

                    // set overall play state to true
                    if (gridState.patterns[patternNum].playing) gridState.playing = true;
                    // if pattern was toggled off, reset tick and reduce number of playing patterns
                    if (!gridState.patterns[patternNum].playing) {
                        gridState.patterns[patternNum].tickReset();
                        gridState.numberOfPlayingPatterns--;
                    }
                    else {
                        // increase number of playing patterns
                        gridState.numberOfPlayingPatterns++;
                    }
                }
            }
        }

        // ensure that the grid gets updated any time the selected pattern changes. 
        //copyCurrentPatternGridEnabledToGridColor();

        if (gridState.numberOfPlayingPatterns == 0) {
            gridState.playing = false;
        }
    }

    if ((int)message->at(0) == 176 && (int)message->at(2) == 0 && (int)message->at(1) < 90 && (int)message->at(1) > 20) {
        time_t playButtonUpTime = dateNowMillis();
        if (playButtonUpTime - playButtonDownTime > 1000) {
            //gridState.gridMode = "patternOpts1";
            //sendScrollTextToLP(gridMidiOut, "X steps",127,127,127,15);
        }
    }

    
    if(gridState.gridMode == "normal"){
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) { // grid button
            gridButtonDownTime[gridX + (8 * gridY)] = dateNowMillis();
            gridState.patterns[gridState.currentSelectedPattern].toggleButton(gridX, gridY);
            //copyCurrentPatternGridEnabledToGridColor();
            for (int i = 0; i < 9; i++) {
                gridButtonsPressed[i] = gridButtonsPressed[i + 1];
            }
            gridButtonsPressed[9] = (int)message->at(1);
            lastPressedGridButton[0] = gridX;
            lastPressedGridButton[1] = gridY;
        }
        if ((int)message->at(0) == 144 && (int)message->at(2) == 0) {
            gridButtonUpTime[gridX + (8 * gridY)] = dateNowMillis();
            int pressedArrayIndex = 0;
            int* el = std::find_if(std::begin(gridButtonsPressed), std::end(gridButtonsPressed), [=](int el) {return el == (int)message->at(1); });
            if (el != std::end(gridButtonsPressed)) {
                pressedArrayIndex = std::distance(gridButtonsPressed, el);
            }

            int numberOfPressedButtons = 0;
            el = std::find_if(std::begin(gridButtonsPressed), std::end(gridButtonsPressed), [=](int el) {return el > 0; });
            if (el != std::end(gridButtonsPressed)) {
                numberOfPressedButtons = 10 - std::distance(gridButtonsPressed, el);
            }
             
            for (int i = pressedArrayIndex; i < 9; i++) {
                gridButtonsPressed[i] = gridButtonsPressed[i + 1];
            }
            gridButtonsPressed[9] = 0;
                        
            // checking that this button release corresponds to a single button being pressed.
            if (numberOfPressedButtons == 1) {
                // check time this button was pressed
                time_t timePressed = gridButtonUpTime[gridX + (8 * gridY)] - gridButtonDownTime[gridX + (8 * gridY)];
                //check for long press
                if (timePressed > 1000) {
                    //enter gridNote options mode
                    gridState.gridMode = "gridNoteOpts";
                    clearGrid();
                    drawGridNoteOpts();
                    tempVelocity = gridState.patterns[gridState.currentSelectedPattern].getVelocity(gridX, gridY);
                    tempNote = gridState.patterns[gridState.currentSelectedPattern].getNote(gridX, gridY);
                    if (!gridState.patterns[gridState.currentSelectedPattern].getNoteEnabled(gridX, gridY,1)) {
                        gridState.patterns[gridState.currentSelectedPattern].toggleButton(gridX, gridY);
                    }
                }
            }
        }
    }else if(gridState.gridMode == "patternOpts1"){
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) {
            int numSteps = (gridY * 8) + gridX + 1;
            gridState.patterns[gridState.currentSelectedPattern].xSize = numSteps;
            gridState.gridMode = "patternOpts2";
            sendScrollTextToLP(gridMidiOut,"Y steps", 127, 127, 127, 15);
        }
    }else if (gridState.gridMode == "patternOpts2"){
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) {
            int numSteps = (gridY * 8) + gridX + 1;
            gridState.patterns[gridState.currentSelectedPattern].ySize = numSteps;
            gridState.gridMode = "patternOpts3";
            sendScrollTextToLP(gridMidiOut, "X step size", 127, 127, 127, 15);
        }
    }
    /*
    case "patternOpts3": {
        if (message[0] == 144 && message[2] == 127) {
            gridState.patterns[gridState.currentSelectedPattern].stepSizeX = ((gridY * 8) + gridX + 1);
            gridState.gridMode = "patternOpts4";
            sendScrollTextToLaunchPad("Y step size", 15);
        }
        break;
    }
    case "patternOpts4": {
        if (message[0] == 144 && message[2] == 127) {
            gridState.patterns[gridState.currentSelectedPattern].stepSizeY = ((gridY * 8) + gridX + 1);
            gridState.gridMode = "normal";
            copyCurrentPatternGridEnabledToGridColor();
            // sendScrollTextToLaunchPad("Y step size", 15);
        }
        break;
    }
    case "patternOpts5": {
        break;
    }
    case "patternOpts6": {
        break;
    }
    case "patternOpts7": {
        break;
    }
    case "patternOpts8": {
        copyCurrentPatternGridEnabledToGridColor();
        break;
    }*/
    else if(gridState.gridMode == "gridNoteOpts"){
        // each button in this mode will set a temporary value that gets set when the confirm / ok button is pressed. 
        // for the buttons that set velocity, change the velocity settings for the slected note when the button is pressed.
        if (gridY == 4) {
            tempVelocity = (gridX * 8) + 7;
        }
        else if (gridY == 5) {
            tempVelocity = ((gridX + 8) * 8) + 7;
        }
        else if (gridY == 7) {
            if (gridX == 0) {
                gridState.gridMode = "normal";
                clearGrid();
                copyCurrentPatternGridEnabledToGridColor();
            }
            else if (gridX == 7) {
                gridState.gridMode = "normal";
                gridState.patterns[gridState.currentSelectedPattern].setVelocity(lastPressedGridButton[0], lastPressedGridButton[1], tempVelocity);
                gridState.patterns[gridState.currentSelectedPattern].setNote(lastPressedGridButton[0], lastPressedGridButton[1], tempNote);
                gridState.patterns[gridState.currentSelectedPattern].setNoteLength(lastPressedGridButton[0], lastPressedGridButton[1], calculateNoteLength(tempnoteLength[0], tempnoteLength[1]));
                clearGrid();
                copyCurrentPatternGridEnabledToGridColor();
            }
        }
        // console.log({tempVelocity})

        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) {
            // note-on message
            // check for note buttons on grid
            if (gridY == 1) {
                int naturalNoteOffsets[9] = { 0, 2, 4, 5, 7, 9, 11, 12 , 0};
                tempNote = naturalNoteOffsets[gridX] + (tempOctave * 12);
                // console.log({tempNote})
                playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, tempVelocity, gridState.patterns[gridState.currentSelectedPattern].getNoteLength(lastPressedGridButton[0], lastPressedGridButton[1]));
            }
            else if (gridY == 2) {
                int sharpFlatNoteOffsets[9] = { 0, 1, 3, 3, 6, 8, 10, 12 , 0};
                tempNote = sharpFlatNoteOffsets[gridX] + (tempOctave * 12);
                playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, tempVelocity, gridState.patterns[gridState.currentSelectedPattern].getNoteLength(lastPressedGridButton[0], lastPressedGridButton[1]));
                // console.log({tempNote})
            }
            else if (gridY == 0) {
                tempOctave = gridX + 1;
            }
            else if (gridY == 6) {
                // tempnoteLength = gridX + 1;
                gridButtonDownTime[gridX + (8 * gridY)] = dateNowMillis();
                for (int i = 0; i < 9; i++) {
                    gridButtonsPressed[i] = gridButtonsPressed[i + 1];
                }
                gridButtonsPressed[9] = (int)message->at(1);
            }

        }
        else if ((int)message->at(0) == 144 && (int)message->at(2) == 0) {
            if (gridY == 1) {
                int naturalNoteOffsets[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
                if (gridX < 8) {
                    tempNote = naturalNoteOffsets[gridX] + (tempOctave * 12);
                }
                playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, 0, -1);
            }
            else if (gridY == 2) {
                int sharpFlatNoteOffsets[8] = { 0, 1, 3, 3, 6, 8, 10, 12 };
                if (gridX < 8) {
                    tempNote = sharpFlatNoteOffsets[gridX] + (tempOctave * 12);
                }
                playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, 0, -1);
            }
            else if (gridY == 6) {
                // tempnoteLength = gridX + 1;
                gridButtonUpTime[gridX + (8 * gridY)] = dateNowMillis();
                /*int pressedArrayIndex = gridButtonsPressed.findIndex(el = > el == message[1]);
                let numberOfPressedButtons = 10 - gridButtonsPressed.findIndex(el = > el > 0);
                gridButtonsPressed.splice(pressedArrayIndex, 1);
                gridButtonsPressed.unshift(0);
                // console.log({numberOfPressedButtons})
                // checking that this button release corresponds to a single button being pressed.
                if (numberOfPressedButtons == 1) {
                    // check time this button was pressed
                    let timePressed = gridButtonUpTime[gridX + (8 * gridY)] - gridButtonDownTime[gridX + (8 * gridY)];
                    // console.log({timePressed});
                    //check for long press
                    if (timePressed > 1000) {
                        //enter gridNote options mode
                        // gridState.gridMode = "gridNoteOpts";
                        // clearGrid();
                        // drawGridNoteOpts();
                        // tempVelocity = gridState.patterns[gridState.currentSelectedPattern].getVelocity(gridX, gridY);
                        // tempNote = gridState.patterns[gridState.currentSelectedPattern].getNote(gridX, gridY);
                        //console.log("break")
                        tempnoteLength[0] = gridX;
                    }
                    else {
                        tempnoteLength[1] = gridX;
                    }
                }*/
            }
        }
    }
}

/* sets the Launchpad to programmer mode */
void setLPtoProgrammerMode(RtMidiOut* rtmidiout) {
    std::vector<unsigned char> message;
    message.push_back(240);
    message.push_back(0);
    message.push_back(32);
    message.push_back(41);
    message.push_back(2);
    message.push_back(13);
    message.push_back(14);
    message.push_back(1);
    message.push_back(247);
    rtmidiout->sendMessage(&message);
    return;
}

/* Sends welcome message that scolls across the grid. This is a blocking function. */
void sendWelcomeMessage(RtMidiOut* rtmidiout) {
    if (!settings.welcomeMessageEnabled)return;
    std::string welcomeMessage = "GRID MIDI";
    sendScrollTextToLP(rtmidiout, welcomeMessage, 0,127,127,10);
    double frequency = 0.3;
    for (int i = 0; i < 32; i++) {
        int red = int(sin(frequency * i + 0) * 63 + 64) | 0;
        int grn = int(sin(frequency * i + 2) * 63 + 64) | 0;
        int blu = int(sin(frequency * i + 4) * 63 + 64) | 0;
        sendScrollTextToLP(rtmidiout, "", red, grn, blu, 10);
        Sleep(170);
    }
}

void sendScrollTextToLP(RtMidiOut* rtmidiout, std::string textToSend, uint8_t red, uint8_t grn, uint8_t blu, uint8_t speed) {
    std::vector<unsigned char> message;
    message.push_back(240);
    message.push_back(0);
    message.push_back(32);
    message.push_back(41);
    message.push_back(2);
    message.push_back(13);
    message.push_back(7);
    message.push_back(0);
    message.push_back(speed);
    message.push_back(1);
    message.push_back(red);
    message.push_back(grn);
    message.push_back(blu);
    for (uint8_t i = 0; i < textToSend.size(); i++) {
        message.push_back(textToSend[i]);
    }
    message.push_back(247);
    rtmidiout->sendMessage(&message);
}

time_t dateNowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

time_t dateNowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* sends all of the grid colors to the LP */
time_t sendColors(time_t lastSentTime, RtMidiOut* rtmidiout, bool overrideThrottle) {
    time_t currentTime = dateNowMillis();
    if (currentTime - lastSentTime > 10 || overrideThrottle) {
        std::vector<unsigned char> message;
        message.push_back(240);
        message.push_back(0);
        message.push_back(32);
        message.push_back(41);
        message.push_back(2);
        message.push_back(13);
        message.push_back(3);
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                message.push_back(3);
                message.push_back((x + 1) + ((y + 1) * 10));
                message.push_back(gridState.gridColor[x][y].r);
                message.push_back(gridState.gridColor[x][y].g);
                message.push_back(gridState.gridColor[x][y].b);
            }
        }
        for (int i = 1; i <= 8; i++) {
            message.push_back(3);
            message.push_back(90 + i);
            message.push_back(gridState.otherColor[i - 1].r);
            message.push_back(gridState.otherColor[i - 1].g);
            message.push_back(gridState.otherColor[i - 1].b);
        }
        for (int i = 1; i <= 8; i++) {
            message.push_back(3);
            message.push_back((i * 10) + 9);
            message.push_back(gridState.otherColor[i + 7].r);
            message.push_back(gridState.otherColor[i + 7].g);
            message.push_back(gridState.otherColor[i + 7].b);
        }
        message.push_back(247);
        rtmidiout->sendMessage(&message);
        std::vector<unsigned char>mes;
        mes.push_back(240);
        mes.push_back(0);
        mes.push_back(32);
        mes.push_back(41);
        mes.push_back(2);
        mes.push_back(13);
        mes.push_back(3);
        mes.push_back(3);
        mes.push_back(99);
        mes.push_back(gridState.logoColor.r);
        mes.push_back(gridState.logoColor.g);
        mes.push_back(gridState.logoColor.b);
        mes.push_back(247);
        rtmidiout->sendMessage(&mes);
        return currentTime;
    }
    else {
        return 0;
    }
}

void copyCurrentPatternGridEnabledToGridColor() {
    if (gridState.gridMode != "normal")return;
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 0;

            if (gridState.patterns[gridState.currentSelectedPattern].getCurrentGridX() == x) {
                gridState.gridColor[x][y].r = 10;
                gridState.gridColor[x][y].g = 10;
                gridState.gridColor[x][y].b = 10;
            }

            if (gridState.patterns[gridState.currentSelectedPattern].getCurrentGridY() == y) {
                gridState.gridColor[x][y].r = 10;
                gridState.gridColor[x][y].g = 10;
                gridState.gridColor[x][y].b = 10;
            }

            if (gridState.patterns[gridState.currentSelectedPattern].getNoteEnabled(x, y, 1)) {
                gridState.gridColor[x][y].b = 127;
            }
        }
    }
}

void clearGrid() {
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 0;
        }
    }
}

std::future<void> noteOffFuturesArray[128];
int noteOffFuturesArrayIndex = 0;

void playNoteGlobal(int portI, int chan, int note, int vel, int len) {
    // check that this should be sent to midi device
    if (portI < 128) {
        // check that the midi device is enabled
        if (midiOutputDevicesEnabled[portI]) {
            if (vel > 0) {
                // send the note to the midi device
                std::vector<unsigned char>message;
                message.push_back(noteOnMessage | chan);
                message.push_back(note);
                message.push_back(vel);
                midiOut[portI]->sendMessage(&message);
                // check for -1 which means that the note gets turned off manually
                if (len != -1) {
                    currentPlayingNotes[currentPlayingNotesIndex].channel = chan;
                    currentPlayingNotes[currentPlayingNotesIndex].note = note;
                    currentPlayingNotes[currentPlayingNotesIndex].portIndex = portI;
                    playingNotesExpireTimes[currentPlayingNotesIndex] = dateNowMillis() + len;
                    currentPlayingNotesIndex++;
                    if (currentPlayingNotesIndex >= 1024)currentPlayingNotesIndex = 0;
                }
            }
            else {
                // if velocity is 0, the note should turned off, send note-off
                std::vector<unsigned char>message;
                message.push_back(noteOffMessage | chan);
                message.push_back(note);
                message.push_back(0);
                midiOut[portI]->sendMessage(&message);
            }
        }
    }
    else if (portI < 1100) {
        // @todo 
        // do CV-gate output
    }
    else if (portI < 1108) {
        // @todo 
        // do drum gate output
    }       
}

int calculateNoteLength(int noteLengthIndex_x, int noteLengthIndex_y) {

    double noteLengthArray[8][8] = {
        {1.0 / 24, 2.0 / 24, 3.0 / 24, 4.0 / 24, 5.0 / 24, 6.0 / 24, 7.0 / 24, 8.0 / 24}, // sixteenth triplets
        {1.0 / 16, 2.0 / 16, 3.0 / 16, 4.0 / 16, 5.0 / 16, 6.0 / 16, 7.0 / 16, 8.0 / 16}, // sixteenths
        {1.0 / 12, 2.0 / 12, 3.0 / 12, 4.0 / 12, 5.0 / 12, 6.0 / 12, 7.0 / 12, 8.0 / 12 }, // triplets
        {1.0 / 8, 2.0 / 8, 3.0 / 8, 4.0 / 8, 5.0 / 8, 6.0 / 8, 7.0 / 8, 8.0 / 8}, // eigths
        {1.0 / 6,  2.0 / 6,  3.0 / 6, 4.0 / 6, 5.0 / 6, 6.0 / 6, 7.0 / 6, 8.0 / 6}, // quater triplets
        {1.0 / 4,  2.0 / 4,  3.0 / 4, 4.0 / 4, 5.0 / 4, 6.0 / 4, 7.0 / 4, 8.0 / 4}, // qarters
        {1.0 / 3,  2.0 / 3,  3.0 / 3, 4.0 / 3, 5.0 / 3, 6.0 / 3, 7.0 / 3, 8.0 / 3}, // half note triplets
        {1.0 / 2,  2.0 / 2,  3.0 / 2, 4.0 / 2, 5.0 / 2, 6.0 / 2, 7.0 / 2, 8.0 / 2} // half notes
    };
    double secondsPerMeasure = 60 / (double(gridState.bpm) / 4);
    double noteTime = secondsPerMeasure * noteLengthArray[noteLengthIndex_x][noteLengthIndex_y];
    return int(noteTime * 1000);
}


void drawGridNoteOpts() {
    // cancel, top left
    gridState.gridColor[0][7].r = 127;
    // ok, top right
    gridState.gridColor[7][7].g = 127;
    // note length, row 2
    for (int x = 0; x < 8; x++) {
        gridState.gridColor[x][6].r = 127;
        gridState.gridColor[x][6].b = 0;
        gridState.gridColor[x][6].g = 127;
    }
    // octaves, bottom row
    for (int x = 0; x < 8; x++) {
        gridState.gridColor[x][0].r = 127;
        gridState.gridColor[x][0].b = 127;
    }
    // keyboard, C's
    gridState.gridColor[7][1].r = 127;
    gridState.gridColor[7][1].b = 127;
    gridState.gridColor[0][1].r = 127;
    gridState.gridColor[0][1].b = 127;
    // keyboard, naturals
    for (int x = 1; x < 7; x++) {
        gridState.gridColor[x][1].r = 127;
        gridState.gridColor[x][1].b = 127;
        gridState.gridColor[x][1].g = 127;
    }
    //keyboard, sharps/flats
    gridState.gridColor[1][2].r = 127;
    gridState.gridColor[1][2].b = 127;
    gridState.gridColor[1][2].g = 127;

    gridState.gridColor[2][2].r = 127;
    gridState.gridColor[2][2].b = 127;
    gridState.gridColor[2][2].g = 127;

    gridState.gridColor[4][2].r = 127;
    gridState.gridColor[4][2].b = 127;
    gridState.gridColor[4][2].g = 127;

    gridState.gridColor[5][2].r = 127;
    gridState.gridColor[5][2].b = 127;
    gridState.gridColor[5][2].g = 127;

    gridState.gridColor[6][2].r = 127;
    gridState.gridColor[6][2].b = 127;
    gridState.gridColor[6][2].g = 127;

    // velocity, rows 4,5
    for (int x = 0; x < 8; x++) {
        for (int y = 4; y <= 5; y++) {
            int val = ((((((y - 4) * 8) + x) / 2) * ((((y - 4) * 8) + x) / 2)) | 0) + 5;
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].b = val;
            gridState.gridColor[x][y].g = val;
        }
    }
} 

void updateMidiDevices() {
    try {
        numMidiInDevs = midiIn[127]->getPortCount();
        if (numMidiInDevs > 128)std::cout << "Only able to handle first 128 midi input devices. " << std::endl;
        std::string portName;
        // go through all available midi devices and look for the LPminiMK3
        for (uint16_t i = 0; i < (numMidiInDevs<128?numMidiInDevs:128); i++) {
            portName = midiIn[i]->getPortName(i);
            std::string LPminiMK3_name = "MIDIIN2 (LPMiniMK3 MIDI)";
            int val = portName.compare(0, LPminiMK3_name.size(), LPminiMK3_name);
            midiInPortNames[i] = portName;
            if (val == 0) {
                if (gridMidiInDevIndex == -1) {
                    
                    gridMidiIn->setCallback(&gridMidiInCB);
                    gridMidiIn->openPort(i);
                    // Don't ignore sysex, timing, or active sensing messages.
                    gridMidiIn->ignoreTypes(true, true, true);
                    gridMidiInDevIndex = i;
                }
            }
            else {
                if (midiInputDevicesEnabled[i] && !midiIn[i]->isPortOpen()) {
                    midiIn[i]->openPort(i);
                    midiIn[i]->ignoreTypes(false, false, false);
                }
                else if (!midiInputDevicesEnabled[i] && midiIn[i]->isPortOpen()) {
                    midiIn[i]->closePort();
                }
            }
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
    try {
        numMidiOutDevs = midiOut[127]->getPortCount();
        if (numMidiOutDevs > 128)std::cout << "Only able to handle first 128 midi output devices. " << std::endl;
        std::string portName;
        //go through all midi out devices and look for LPminiMK3
        for (uint16_t i = 0; i < (numMidiOutDevs<128?numMidiOutDevs:128); i++) {
            portName = midiOut[i]->getPortName(i);
            
            std::string LPminiMK3_name = "MIDIOUT2 (LPMiniMK3 MIDI)";
            int val = portName.compare(0, LPminiMK3_name.size(), LPminiMK3_name);
            midiOutPortNames[i] = portName;
            if (val == 0) {
                if (gridMidiOutDevIndex == -1) {
                    gridMidiOut->openPort(i);
                    gridMidiOutDevIndex = i;
                    programState.launchPadInit = true;
                }
            }
            else {
                if (midiOutputDevicesEnabled[i] && !midiOut[i]->isPortOpen()) {
                    midiOut[i]->openPort(i);
                }
                else if (!midiOutputDevicesEnabled[i] && midiOut[i]->isPortOpen()) {
                    midiOut[i]->closePort();
                }
            }
            //std::cout << portName << std::endl;
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
}