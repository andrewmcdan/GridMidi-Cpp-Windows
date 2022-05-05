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
#include "RtMidi.h"

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/);
void setLPtoProgrammerMode(RtMidiOut* rtmidiout);
void sendWelcomeMessage(RtMidiOut* rtmidiout);
void sendScrollTextToLP(RtMidiOut* rtmidiout, std::string textToSend, uint8_t red, uint8_t grn, uint8_t blu, uint8_t speed);
time_t dateNowMillis();
time_t dateNowMicros();
time_t sendColors(time_t lastSentTime, bool overrideThrottle = false);

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



class gridPattern_XY_OR {
public:
    void setCallBacks(std::function<void()>flashPlayCB, std::function<void()>playBtnOnCB, std::function<void()>playBtnOffCB) {
        playBtnOnCB = playBtnOnCB;
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
    void toggleButton(int x, int y) {
        std::cout << "toggle " << x << ", " << y << std::endl;
        // unfinished
    }

    std::function<void()>playButtonOn;
    std::function<void()>playButtonOff;
    std::function<void()>flashPlayButton;
private:
    bool playing = false;
    std::vector<std::vector<gridElement_t>>grid;
    int xSize = 8;
    int ySize = 16;
    int xViewIndex = 0;
    int yViewIndex = 0;
    int currentXstep = 0;
    int currentYstep = 0;
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
        logoColor.r = 127;
        logoColor.g = 127;
        logoColor.b = 127;

        for (int i = 0; i < 7; i++) {
            patterns.push_back(gridPattern_XY_OR());
            patterns[i].setCallBacks(
                [this, i] () {
                otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 127 : 0;
                otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 127 : 0;
                }, 
                [this, i] () {
                    otherColor[15 - i].r = 10;
                    otherColor[15 - i].g = 0;
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
    color_t logoColor;
    int bpm = 120;
    std::string gridMode = "normal";
    int currentSelectedPattern = 0;
    int numberOfPlayingPatterns = 0;
    std::vector<gridPattern_XY_OR>patterns;
};


int main(int argc, char *argv[])
{
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
            }
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }

    gridPattern_XY_OR pat;

    setLPtoProgrammerMode(gridMidiOut);
    sendWelcomeMessage(gridMidiOut);

    time_t lastSendColorTime = dateNowMillis();
    time_t newSentColorTime = sendColors(lastSendColorTime, false);
    if (newSentColorTime != 0)
        lastSendColorTime = newSentColorTime;

    

    std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
    char input;
    std::cin.get(input);

    for (int i = 0; i < 128; i++) {
        delete midiIn[i];
    }
    delete gridMidiIn;

    return 0;
}

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/)
{
    int nBytes = message->size();
    for (int i = 0; i < nBytes; i++)
        std::cout << "Byte " << i << " = " << (int)message->at(i) << ", ";
    if (nBytes > 0)
        std::cout << "stamp = " << deltatime << std::endl;
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
    float frequency = 0.3;
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
    for (int i = 0; i < textToSend.size(); i++) {
        message.push_back(textToSend[i]);
    }
    message.push_back(247);
    rtmidiout->sendMessage(&message);
}

time_t dateNowMillis() {
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return millis;
}

time_t dateNowMicros() {
    //auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return micros;
}

/* unfinished */
time_t sendColors(time_t lastSentTime, bool overrideThrottle) {
    time_t currentTime = dateNowMillis();
    if (currentTime - lastSentTime > 10 || overrideThrottle) {
        return currentTime;
    }
    else {
        return 0;
    }
}