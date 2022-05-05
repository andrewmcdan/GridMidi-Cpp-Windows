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

const unsigned char noteOnMessage = 0b10010000;
const unsigned char noteOffMessage = 0x80;

void playNoteGlobal(int portI, int chan, int note, int vel, int len);

struct {
    int maxXsize = 64;
    int maxYsize = 64;
} settings;

struct gridElement_t {
    uint8_t enabled;
    int note;
    int velocity;
    int noteLength;
    bool playing;
};

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
    time_t expiration;
    int portIndex;
    int channel;
};

class gridPattern_XY_OR {
public:
    void setCallBacks(std::function<void()>flashPlayCB, std::function<void()>playBtnOnCB, std::function<void()>playBtnOffCB) {
        playButtonOn = playBtnOnCB;
        playButtonOff = playBtnOffCB;
        flashPlayButton = flashPlayCB;
    }
    gridPattern_XY_OR() {
        outputPort.portIndex = 3;
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
        std::cout << "toggle " << x << ", " << y << std::endl;
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
        // @todo
        this->setNotePlaying(x, y, true);
        playNoteGlobal(this->outputPort.portIndex, this->outputPort.channel, this->getNote(x, y), this->getVelocity(x, y), this->getNoteLength(x, y));
        std::async(std::launch::async, [=]()
        {
            Sleep(this->getNoteLength(x,y));
            this->setNotePlaying(x, y, false);
        });
    }

    void playStepX() {
        for (uint8_t i = 0; i < this->grid[this->currentXstep].size(); i++) {
            if (this->getNoteEnabled(this->currentXstep, i, 0)) {
                std::cout << "playXstep" << std::endl;
                this->playNote(this->currentXstep, i);
            }
        }
        this->currentXstep++;
        if (this->currentXstep >= this->xSize)this->currentXstep = 0;
    }

    void playStepY() {
        for (uint8_t i = 0; i < grid.size(); i++) {
            if (this->getNoteEnabled(i, this->currentYstep, 0) && !this->getNotePlaying(i, this->currentYstep)) {
                std::cout << "play y step" << std::endl;
                this->playNote(i, this->currentYstep);
            }
        }
        this->currentYstep++;
        if (this->currentYstep >= this->ySize)this->currentYstep = 0;
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
        this->currentXstep = 0;
        this->currentYstep = 0;
        this->playButtonOn();
    }

    int getCurrentGridX() {
        return this->currentXstep - this->xViewIndex;
    }

    int getCurrentGridY() {
        return this->currentYstep - this->yViewIndex;
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
    int currentXstep = 0;
    int currentYstep = 0;
    bool playing = false;
private:
    std::vector<std::vector<gridElement_t>>grid;
    int xSize = 8;
    int ySize = 16;
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
                otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 50 : 0;
                otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 127 : 0;
                }, 
                [this, i] () {
                    otherColor[15 - i].r = 127;
                    otherColor[15 - i].g = 127;
                    otherColor[15 - i].b = 0;
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

std::vector<playingNote_t>currentPlayingNotes;

int main(int argc, char *argv[])
{
    for (int i = 0; i < 128; i++) {
        midiOutputDevicesEnabled[i] = false;
        midiOutputDevicesClockEn[i] = false;
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
                gridMidiIn->ignoreTypes(false, false, false);
                gridMidiInDevIndex = i;
            }
            else {
                midiIn[i]->openPort(i);
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
                midiOutputDevicesClockEn[i] = true;
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
    int nBytes = message->size();
    for (int i = 0; i < nBytes; i++)
        std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
    if (nBytes > 0)
        std::cout << "stamp = " << deltatime << std::endl;

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
            // @todo probably need to send an update to the grid now
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

void playNoteGlobal(int portI, int chan, int note, int vel, int len) {
    /*playNoteGlobal(this->outputPort.portIndex, this->outputPort.channel, this->getNote(x, y), this->getVelocity(x, y), this->getNoteLength(x, y));
    std::async(std::launch::async, [=]()
    {
        Sleep(this->getNoteLength(x, y));
        this->setNotePlaying(x, y, false);
    });*/

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
                    std::async(std::launch::async, [=]()
                    {
                        Sleep(len);
                        std::vector<unsigned char>message;
                        message.push_back(noteOffMessage | chan);
                        message.push_back(note);
                        message.push_back(0);
                        midiOut[portI]->sendMessage(&message);
                        return;
                    });
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