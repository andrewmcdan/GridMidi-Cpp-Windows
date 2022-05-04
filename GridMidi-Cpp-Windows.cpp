// cspell:disable
//*****************************************//
//  cmidiin.cpp
//  by Gary Scavone, 2003-2004.
//
//  Simple program to test MIDI input and
//  use of a user callback function.
//
//*****************************************//

#include <windows.h>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <sys/utime.h>
#include <ctime>
#include "RtMidi.h"

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/)
{
    int nBytes = message->size();
    for (int i = 0; i < nBytes; i++)
        std::cout << "Byte " << i << " = " << (int) message->at(i) << ", ";
    if (nBytes > 0)
        std::cout << "stamp = " << deltatime << std::endl;
}

void setLPtoProgrammerMode(RtMidiOut* rtmidiout);
void sendWelcomeMessage(RtMidiOut* rtmidiout);
void sendScrollTextToLP(RtMidiOut* rtmidiout, std::string textToSend, uint8_t red, uint8_t grn, uint8_t blu, uint8_t speed);
time_t dateNowMillis();
time_t dateNowMicros();
time_t sendColors(time_t lastSentTime, bool overrideThrottle = false);

int main(int argc, char *argv[])
{
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

    setLPtoProgrammerMode(gridMidiOut);
    sendWelcomeMessage(gridMidiOut);

    time_t lastSendColorTime = dateNowMillis();
    time_t newSentColorTime = sendColors(lastSendColorTime, false);
    if (newSentColorTime != 0)
        lastSendColorTime = newSentColorTime;


    std::cout << "\nReading MIDI input ... press <enter> to quit.\n";
    char input;
    std::cin.get(input);

cleanup:

    for (int i = 0; i < 128; i++) {
        delete midiIn[i];
    }
    delete gridMidiIn;

    return 0;
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
        std::vector<unsigned char> message;
        message.push_back(240);
        message.push_back(0);
        message.push_back(32);
        message.push_back(41);
        message.push_back(2);
        message.push_back(13);
        message.push_back(7);
        message.push_back(0);
        message.push_back(10);
        message.push_back(1);
        message.push_back(red);
        message.push_back(grn);
        message.push_back(blu);
        message.push_back(247);
        rtmidiout->sendMessage(&message);
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