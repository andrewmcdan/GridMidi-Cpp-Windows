// cspell:disable
//*****************************************//
//  cmidiin.cpp
//  by Gary Scavone, 2003-2004.
//
//  Simple program to test MIDI input and
//  use of a user callback function.
//
//*****************************************//

#include <functional>
#include <windows.h>
//#include <stdlib.h>
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

// these constants get OR'ed with the channel when creating MIDI messages.
const unsigned char noteOnMessage = 0b10010000;
const unsigned char noteOffMessage = 0x80;

// Play a note through a midi device
// portI:   index of the midi device in the midiout array
// chan:    desired midi channel
// note:    desired note value
// vel:     note velocity
// len:     note length in milliseconds
void playNoteGlobal(int portI, int chan, int note, int vel, int len);

// settings struct for globally used settings values.
const struct {
    int maxXsize = 64;
    int maxYsize = 64;
    bool welcomeMessageEnabled = false;
} settings;

// struct for holding the data for each note on the grid
struct gridElement_t {
    uint8_t enabled;
    int note;
    int velocity;
    int noteLength;
    bool playing;
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

class gridPattern_XY_OR {
public:
    // Set the callback functions for colring the play button
    void setCallBacks(std::function<void()>flashPlayCB, std::function<void()>playBtnOnCB, std::function<void()>playBtnOffCB) {
        playButtonOn = playBtnOnCB;
        playButtonOff = playBtnOffCB;
        flashPlayButton = flashPlayCB;
    }
    // gridPattern_XY_OR constructor
    // Generate grid with gridElement's and set default values.
    gridPattern_XY_OR() {
        outputPort.portIndex = 1;
        outputPort.channel = 0;
        color.r = 127;
        color.g = 0;
        color.b = 0;
        for (int i = 0; i < settings.maxXsize; i++) {
            grid.push_back(std::vector<gridElement_t>());
            for (int u = 0; u < settings.maxYsize; u++) {
                grid[i].push_back(gridElement_t());
                grid[i][u].enabled = 0;
                grid[i][u].note = 60;
                grid[i][u].velocity = 100;
                grid[i][u].noteLength = 250;
                grid[i][u].playing = false;
            }
        }
    }
    bool toggleButton(int x, int y) {
        //std::cout << "toggle " << x << ", " << y << std::endl;
        if ((x + this->xViewIndex) < this->xSize && (y + this->yViewIndex) < this->ySize) {
            if (this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled == 1) {
                this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled = 0;
            }
            else {
                this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled = 1;
            }
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
    int getNote(int x, int y) {
        return this->grid[x][y].note;
    }
    void setNoteLength(int x, int y, int val) {
        if (val < 15000) {
            this->grid[x][y].noteLength = val;
        }
    }
    int getNoteLength(int x, int y) {
        return this->grid[x][y].noteLength;
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
        for (uint8_t i = 0; i < this->grid[this->currentXstep].size(); i++) {
            if (this->getNoteEnabled(this->currentXstep, i, 0)) {
                this->playNote(this->currentXstep, i);
            }
        }
    }

    void playStepY() {
        this->currentYstep++;
        if (this->currentYstep >= this->ySize)this->currentYstep = 0;
        for (uint8_t i = 0; i < grid.size(); i++) {
            if (this->getNoteEnabled(i, this->currentYstep, 0) && !this->getNotePlaying(i, this->currentYstep)) {
                //std::cout << "play y step" << std::endl;
                this->playNote(i, this->currentYstep);
            }
        }
    }

    void tick() {
        if (this->tickCount % 24 == 0) {
            this->flashPlayButton();
        }

        if (this->tickCount % this->stepSizeX == 0) {
            this->playStepX();
        }

        if (this->tickCount % this->stepSizeY == 0) {
            this->playStepY();
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

    bool setOutPort(int val = -1) {
        if (val != -1) {
            // @todo 
            // set port if enabled, return false if not.
        }
        else {
            return false;
        }
    }

    void turnOnPlayButton() {
        this->playButtonOn();
    }

    std::function<void()>playButtonOn;
    std::function<void()>playButtonOff;
    std::function<void()>flashPlayButton;
    int currentXstep = 64;
    int currentYstep = 8;
    bool playing = false;
private:
    std::vector<std::vector<gridElement_t>>grid;
    std::future<void>playNoteResetFuturesArray[1024];
    int playNoteFuturesArrayIndex = 0;
    int xSize = 64;
    int ySize = 8;
    int xViewIndex = 0;
    int yViewIndex = 0;
    uint32_t tickCount = 0;
    int stepSizeX = 24;
    int stepSizeY = 24;
    uint32_t tickResetVal = 2000000;
    outPort_t outputPort;
    color_t color;
};

struct gS_t {
    gS_t() {
        for (int i = 0; i < 16; i++) {
            otherColor.push_back(color_t());
            otherColor[i].r = 10;
            otherColor[i].g = 0;
            otherColor[i].b = 0;
        }

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
            patterns.push_back(gridPattern_XY_OR());
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
    int bpm = 120;
    std::string gridMode = "normal";
    int currentSelectedPattern = 0;
    int numberOfPlayingPatterns = 0;
    std::vector<gridPattern_XY_OR>patterns;
    bool playing = false;
};

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/);
void setLPtoProgrammerMode(RtMidiOut* rtmidiout);
void sendWelcomeMessage(RtMidiOut* rtmidiout);
void sendScrollTextToLP(RtMidiOut* rtmidiout, std::string textToSend, uint8_t red, uint8_t grn, uint8_t blu, uint8_t speed);
time_t dateNowMillis();
time_t dateNowMicros();
time_t sendColors(time_t lastSentTime, RtMidiOut* rtmidiout, bool overrideThrottle);
void copyCurrentPatternGridEnabledToGridColor();
void clearGrid(gS_t& gridState);

gS_t gridState;

RtMidiIn* gridMidiIn = 0; // RTmidi object for LPminiMK3
RtMidiIn* midiIn[128]; // RTmidi objects for all other midi devs
uint16_t gridMidiInDevIndex = 0; // indicates which element in midiIn[] will be uninitialized due to it correlating to LPminiMK3
uint16_t numMidiInDevs = 0;
std::string midiInPortNames[128];

RtMidiOut* gridMidiOut = 0; // RtMidi object for LPminiMK3
RtMidiOut* midiOut[128]; // RtMidi objects for all other midi devs
uint16_t gridMidiOutDevIndex = 0;
uint16_t numMidiOutDevs = 0;
std::string midiOutPortNames[128];
bool midiOutputDevicesEnabled[128];
bool midiOutputDevicesClockEn[128];

playingNote_t currentPlayingNotes[1024];
time_t playingNotesExpireTimes[1024];
int currentPlayingNotesIndex = 0;

int main(int argc, char *argv[])
{    
    for (int i = 0; i < 128; i++) {
        midiOutputDevicesEnabled[i] = false;
        midiOutputDevicesClockEn[i] = false;
    }

    for (int i = 0; i < 1024; i++) {
        playingNotesExpireTimes[i] = 0;
    }
    
    try {
        // initialize all the placeholders for midi devices.
        for (int i = 0; i < 128; i++) {
            midiIn[i] = new RtMidiIn();
        }
        
        // init gridmidiIn
        gridMidiIn = new RtMidiIn();
        numMidiInDevs = gridMidiIn->getPortCount();
        std::string portName;
        
        // go through all available midi devices and look for the LPminiMK3
        for (uint16_t i = 0; i < numMidiInDevs; i++) {
            portName = gridMidiIn->getPortName(i);
            std::string LPminiMK3_name = "MIDIIN2 (LPMiniMK3 MIDI)";
            int val = portName.compare(0, LPminiMK3_name.size(), LPminiMK3_name);
            midiInPortNames[i] = portName;
            if (val == 0) {
                gridMidiIn->openPort(i);
                gridMidiIn->setCallback(&gridMidiInCB);
                // Don't ignore sysex, timing, or active sensing messages.
                gridMidiIn->ignoreTypes(true, true, true);
                gridMidiInDevIndex = i;
            }
            else {
                midiIn[i]->openPort(i);
                midiIn[i]->ignoreTypes(false, false, false);
            }
        }
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
        numMidiOutDevs = gridMidiOut->getPortCount();
        std::string portName;
        //go through all midi out devices and look for LPminiMK3
        for (uint16_t i = 0; i < numMidiOutDevs; i++) {
            portName = gridMidiOut->getPortName(i);
            std::cout << portName << std::endl;
            std::string LPminiMK3_name = "MIDIOUT2 (LPMiniMK3 MIDI)";
            int val = portName.compare(0, LPminiMK3_name.size(), LPminiMK3_name);
            //std::cout << "portNAME: " << portName << std::endl << "compare result: " << val << std::endl;
            midiOutPortNames[i] = portName;
            if (val == 0) {
                gridMidiOut->openPort(i);
                gridMidiOutDevIndex = i;
            }
            else {
                midiOut[i]->openPort(i);
                midiOutputDevicesEnabled[i] = true;
                midiOutputDevicesClockEn[i] = false;
            }
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }

    gridPattern_XY_OR pat;

    setLPtoProgrammerMode(gridMidiOut);
    sendWelcomeMessage(gridMidiOut);
    copyCurrentPatternGridEnabledToGridColor();

    time_t tempTime = dateNowMicros();
    time_t timerTime = dateNowMicros();
    time_t interval_1ms = 0;
    time_t interval_30ms = 0;
    time_t interval_2s = 0;

    std::cout << "Press any key to exit." << std::endl;

    time_t tickTimer_1 = dateNowMillis();

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
        }

        if (interval_2s > 2000000) {
            interval_2s = 0;
            //gridState.patterns[gridState.currentSelectedPattern].toggleButton(1,1);
            //std::cout << "2 seconds" << std::endl;
        }
        
        if (_kbhit())
            break;
        auto x = std::chrono::steady_clock::now() + std::chrono::microseconds(10);
        std::this_thread::sleep_until(x);
    }

    for (int i = 0; i < 128; i++) {
        delete midiIn[i];
    }
    delete gridMidiIn;
    std::cout << std::endl;
    return 0;
}


time_t gridButtonDownTime[64];
time_t gridButtonUpTime[64];
std::vector<int>gridButtonsPressed = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int lastPressedGridButton[2] = { 0, 0 };
int tempVelocity = 0;
int tempNote = 0;
int tempOctave = 5;
int tempnoteLength[2] = { 0, 0 };
time_t playButtonDownTime = dateNowMillis();

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
            patternNum = 0;
            break;
        }
        case 79: {
            // second play button
            patternNum = 1;
            break;
        }
        case 69: {
            patternNum = 2;
            break;
        }
        case 59: {
            patternNum = 3;
            break;
        }
        case 49: {
            patternNum = 4;
            break;
        }
        case 39: {
            patternNum = 5;
            break;
        }
        case 29: {
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

        // ensure that the grid gets updated any time the selected pattern changes. 
        copyCurrentPatternGridEnabledToGridColor();

        if (gridState.numberOfPlayingPatterns == 0) {
            gridState.playing = false;
        }
    }

    if ((int)message->at(0) == 176 && (int)message->at(2) == 0 && (int)message->at(1) < 90 && (int)message->at(1) > 20) {
        time_t playButtonUpTime = dateNowMillis();
        if (playButtonUpTime - playButtonDownTime > 1000) {
            gridState.gridMode = "patternOpts1";
            sendScrollTextToLP(gridMidiOut, "X steps",127,127,127,15);
        }
    }

    
    if(gridState.gridMode == "normal"){
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) { // grid button
            gridButtonDownTime[gridX + (8 * gridY)] = dateNowMillis();
            gridState.patterns[gridState.currentSelectedPattern].toggleButton(gridX, gridY);
            copyCurrentPatternGridEnabledToGridColor();
            gridButtonsPressed.push_back((int)message->at(1));
            //gridButtonsPressed.shift();
            gridButtonsPressed.erase(gridButtonsPressed.begin());
            lastPressedGridButton[0] = gridX;
            lastPressedGridButton[1] = gridY;
        }
        if ((int)message->at(0) == 144 && (int)message->at(2) == 0) {
            gridButtonUpTime[gridX + (8 * gridY)] = dateNowMillis();
            int pressedArrayIndex = 0;
            auto result = std::find_if(gridButtonsPressed.begin(), gridButtonsPressed.end(), [=](int el) {return el == (int)message->at(1); });
            pressedArrayIndex = *result;
            std::cout << pressedArrayIndex << std::endl;
            /*int numberOfPressedButtons = 10 - gridButtonsPressed.findIndex(el = > el > 0);
            gridButtonsPressed.splice(pressedArrayIndex, 1);
            gridButtonsPressed.unshift(0);
            // checking that this button release corresponds to a single button being pressed.
            if (numberOfPressedButtons == 1) {
                // check time this button was pressed
                let timePressed = gridButtonUpTime[gridX + (8 * gridY)] - gridButtonDownTime[gridX + (8 * gridY)];
                // console.log({timePressed});
                //check for long press
                if (timePressed > 1000) {
                    //enter gridNote options mode
                    gridState.gridMode = "gridNoteOpts";
                    clearGrid();
                    //drawGridNoteOpts();
                    tempVelocity = gridState.patterns[gridState.currentSelectedPattern].getVelocity(gridX, gridY);
                    tempNote = gridState.patterns[gridState.currentSelectedPattern].getNote(gridX, gridY);
                    if (!gridState.patterns[gridState.currentSelectedPattern].getNoteEnabled(gridX, gridY)) {
                        gridState.patterns[gridState.currentSelectedPattern].toggleButton(gridX, gridY);
                    }
                }
            }*/
        }
    }/*
    case "patternOpts1": {
        if (message[0] == 144 && message[2] == 127) {
            let numSteps = (gridY * 8) + gridX + 1;
            gridState.patterns[gridState.currentSelectedPattern].xSize = numSteps;
            gridState.gridMode = "patternOpts2";
            sendScrollTextToLaunchPad("Y steps", 15);
        }
        break;
    }
    case "patternOpts2": {
        if (message[0] == 144 && message[2] == 127) {
            let numSteps = (gridY * 8) + gridX + 1;
            gridState.patterns[gridState.currentSelectedPattern].ySize = numSteps;
            gridState.gridMode = "patternOpts3";
            sendScrollTextToLaunchPad("X step size", 15);
        }
        break;
    }
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
    }
    case "gridNoteOpts": {
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
                gridState.patterns[gridState.currentSelectedPattern].setNoteLength(lastPressedGridButton[0], lastPressedGridButton[1], calculateNoteLength(tempnoteLength, gridState.bpm));
                clearGrid();
                copyCurrentPatternGridEnabledToGridColor();
            }
        }
        // console.log({tempVelocity})

        if (message[0] == 144 && message[2] == 127) {
            // note-on message
            // check for note buttons on grid
            if (gridY == 1) {
                let naturalNoteOffsets = [0, 2, 4, 5, 7, 9, 11, 12];
                tempNote = naturalNoteOffsets[gridX] + (tempOctave * 12);
                // console.log({tempNote})
                playNote(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, tempVelocity, gridState.patterns[gridState.currentSelectedPattern].getNoteLength(lastPressedGridButton[0], lastPressedGridButton[1]));
            }
            else if (gridY == 2) {
                let sharpFlatNoteOffsets = [0, 1, 3, 3, 6, 8, 10, 12];
                tempNote = sharpFlatNoteOffsets[gridX] + (tempOctave * 12);
                playNote(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, tempVelocity, gridState.patterns[gridState.currentSelectedPattern].getNoteLength(lastPressedGridButton[0], lastPressedGridButton[1]));
                // console.log({tempNote})
            }
            else if (gridY == 0) {
                tempOctave = gridX + 1;
            }
            else if (gridY == 6) {
                // tempnoteLength = gridX + 1;
                gridButtonDownTime[gridX + (8 * gridY)] = Date.now();
                gridButtonsPressed.push(message[1]);
                gridButtonsPressed.shift();
            }

        }
        else if (message[0] == 144 && message[2] == 0) {
            if (gridY == 1) {
                // @todo play note
                let naturalNoteOffsets = [0, 2, 4, 5, 7, 9, 11, 12];
                tempNote = naturalNoteOffsets[gridX] + (tempOctave * 12);
                // console.log({tempNote})
                playNote(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, 0);
            }
            else if (gridY == 2) {
                let sharpFlatNoteOffsets = [0, 1, 3, 3, 6, 8, 10, 12];
                tempNote = sharpFlatNoteOffsets[gridX] + (tempOctave * 12);
                playNote(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel, tempNote, 0);
                // console.log({tempNote})
            }
            else if (gridY == 6) {
                // tempnoteLength = gridX + 1;
                gridButtonUpTime[gridX + (8 * gridY)] = Date.now();
                let pressedArrayIndex = gridButtonsPressed.findIndex(el = > el == message[1]);
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
                        console.log("break")
                            tempnoteLength[0] = gridX;
                    }
                    else {
                        tempnoteLength[1] = gridX;
                    }
                }
            }
        }
        break;
    }*/
    


















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
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return millis;
}

time_t dateNowMicros() {
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return micros;
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

void clearGrid(gS_t& gridState) {
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
    if (portI < 1000) {
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
                    /*noteOffFuturesArray[noteOffFuturesArrayIndex] = std::async(std::launch::async, [=]() {
                        //std::cout << "send note off" << std::endl;
                        Sleep(len);
                        std::vector<unsigned char>message;
                        message.push_back(noteOffMessage | chan);
                        message.push_back(note);
                        message.push_back(0);
                        midiOut[portI]->sendMessage(&message);
                        
                        return;
                    });
                    noteOffFuturesArrayIndex++;
                    if (noteOffFuturesArrayIndex >= 128)noteOffFuturesArrayIndex = 0;*/
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

int calculateNoteLength(int noteLengthIndex_x, int noteLengthIndex_y, int bpm) {

    float noteLengthArray[8][8] = {
        {1 / 24, 2 / 24, 3 / 24, 4 / 24, 5 / 24, 6 / 24, 7 / 24, 8 / 24}, // sixteenth triplets
        {1 / 16, 2 / 16, 3 / 16, 4 / 16, 5 / 16, 6 / 16, 7 / 16, 8 / 16}, // sixteenths
        {1 / 12, 2 / 12, 3 / 12, 4 / 12, 5 / 12, 6 / 12, 7 / 12, 8 / 12 }, // triplets
        {1 / 8, 2 / 8, 3 / 8, 4 / 8, 5 / 8, 6 / 8, 7 / 8, 8 / 8}, // eigths
        {1 / 6,  2 / 6,  3 / 6, 4 / 6, 5 / 6, 6 / 6, 7 / 6, 8 / 6}, // quater triplets
        {1 / 4,  2 / 4,  3 / 4, 4 / 4, 5 / 4, 6 / 4, 7 / 4, 8 / 4}, // qarters
        {1 / 3,  2 / 3,  3 / 3, 4 / 3, 5 / 3, 6 / 3, 7 / 3, 8 / 3}, // half note triplets
        {1 / 2,  2 / 2,  3 / 2, 4 / 2, 5 / 2, 6 / 2, 7 / 2, 8 / 2} // half notes
    };
    float secondsPerMeasure = 60 / (float(bpm) / 4);
    float noteTime = secondsPerMeasure * noteLengthArray[noteLengthIndex_x][noteLengthIndex_y];
    return int(noteTime * 1000);
}