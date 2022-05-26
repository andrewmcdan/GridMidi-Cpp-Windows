// cspell:disable

/*
* TODO:
* - S̶e̶n̶d̶ m̶i̶d̶i̶ s̶t̶a̶r̶t̶/̶s̶t̶o̶p̶ m̶e̶s̶s̶a̶g̶e̶s̶ w̶h̶e̶n̶ t̶h̶e̶ p̶l̶a̶y̶ s̶t̶a̶t̶e̶ c̶h̶a̶n̶g̶e̶s̶.̶
* - L̶o̶a̶d̶ /̶ s̶a̶v̶e̶ p̶r̶o̶j̶e̶c̶t̶
* - L̶o̶a̶d̶ /̶ s̶a̶v̶e̶ s̶e̶t̶t̶i̶n̶g̶s̶
*/

#include "GridMidi.h"

#define DEBUG_EN

const int DEFAULT_xSize = 12;
const int DEFAULT_ySize = 8;
const int drumNoteValLUT[8][8] = {
    {36,37,38,39,52,53,54,55},
    {40,41,42,43,56,57,58,59},
    {44,45,46,47,60,61,62,63},
    {48,49,50,51,64,65,66,67},
    {68,69,70,71,84,85,86,87},
    {72,73,74,75,88,89,90,91},
    {76,77,78,79,92,93,94,95},
    {80,81,82,83,96,97,98,99}
};

// these constants get OR'ed with the channel when creating MIDI messages.
const unsigned char noteOnMessage = 0b10010000;
const unsigned char noteOffMessage = 0x80;

// program state struct for keeping track of program state
struct {
    bool midiDevsInit = false;
    bool launchPadInit = false;
}programState;

// settings struct for globally used settings values.
const struct {
    int maxXsize = 64;
    int maxYsize = 64;
    int maxXstepSize = 384;
    int maxYstepSize = 384;
#ifdef DEBUG_EN
    bool welcomeMessageEnabled = false;
#else
    bool welcomeMessageEnabled = true;
#endif
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

// struct types for storing various data
struct outPort_t {
    int portIndex;
    int channel;
};
struct color_t {
    int r;
    int g;
    int b;
};
struct playingNote_t {
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
        outputPort.portIndex = -1;
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
            } else {
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
        } else {
            this->grid[x][y].velocity = 127;
        }
    }
    void setVelocity(int x, int y, int val, bool offset) {
        if (offset) {
            this->setVelocity(x, y, val);
        } else {
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
        } else {
            this->grid[x][y].note = 127;
        }
    }
    void setNote(int x, int y, int val, bool offset) {
        if (offset) {
            this->setNote(x, y, val);
        } else {
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
        } else {
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
        } else {
            this->setNoteLength(x + this->xViewIndex, y + this->yViewIndex, val_x, val_y);
        }
    }
    int getNoteLength(int x, int y) {
        if (this->grid[x][y].noteLengthUseFractional) {
            return calculateNoteLength(this->grid[x][y].noteLengthFractional_x, this->grid[x][y].noteLengthFractional_y);
        } else
            return this->grid[x][y].noteLength_ms;
    }
    void getNoteLengthFractional(int x, int y, int* coords) {
        coords[0] = this->grid[x][y].noteLengthFractional_x;
        coords[1] = this->grid[x][y].noteLengthFractional_y;
    }

    bool getIsUsingFractionalNoteLength(int x, int y, bool offset = false) {
        if (offset) {
            return this->grid[x][y].noteLengthUseFractional;
        } else {
            return this->grid[x + this->xViewIndex][y + this->yViewIndex].noteLengthUseFractional;
        }

    }

    // return true/false bassed on note enabled or not
    // if offset == 1, take into account view shift
    bool getNoteEnabled(int x, int y, int offset) {
        if (offset == 1) {
            return this->grid[x + this->xViewIndex][y + this->yViewIndex].enabled == 1;
        } else {
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
        playNoteResetFuturesArray[playNoteFuturesArrayIndex] = std::async(std::launch::async, [=]() {
            Sleep(this->getNoteLength(x, y));
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
        case patternModes::XY_AND:
            if (this->getNoteEnabled(this->currentXstep, this->currentYstep, 0) && !this->getNotePlaying(this->currentXstep, this->currentYstep)) {
                this->playNote(this->currentXstep, this->currentYstep);
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
        if (!this->playingQueued) {
            if (this->tickCount % this->stepSizeX == 0) {
                this->flashPlayButton();
                this->tick_zero_beat = true;
            } else {
                this->tick_zero_beat = false;
            }
            if (this->tickCount % (this->xSize * this->stepSizeX) == 0) {
                this->tick_zero_bar = true;
            } else {
                this->tick_zero_bar = false;
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
                // TODO
                break;
            default:
                break;
            }
            this->tickCount++;
            if (this->tickCount >= this->tickResetVal)this->tickCount = 0;
        }
    }

    void tickReset() {
        this->tickCount = 0;
        this->currentXstep = this->xSize;
        this->currentYstep = this->ySize;
        this->playButtonOn();
    }

    int getCurrentGridX() {
        //if (this->currentXstep - this->xViewIndex - 1 < 0)return this->xSize - 1;
        return this->currentXstep - this->xViewIndex;
    }

    int getCurrentGridY() {
        //if (this->currentYstep - this->yViewIndex - 1 < 0)return this->ySize - 1;
        return this->currentYstep - this->yViewIndex;
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
        if (xNumSteps < settings.maxXsize && yNumSteps < settings.maxYsize && xStepSize < settings.maxXstepSize && yStepSize < settings.maxYstepSize) {
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

    gridElement_t getGridEl(int x, int y) {
        return this->grid[x][y];
    }

    void setGridEl(int x, int y, gridElement_t el) {
        this->grid[x][y].enabled = el.enabled;
        this->grid[x][y].note = el.note;
        this->grid[x][y].noteLengthFractional_x = el.noteLengthFractional_x;
        this->grid[x][y].noteLengthFractional_y = el.noteLengthFractional_y;
        this->grid[x][y].noteLengthUseFractional = el.noteLengthUseFractional;
        this->grid[x][y].noteLength_ms = el.noteLength_ms;
        this->grid[x][y].velocity = el.velocity;
    }

    std::function<void()>playButtonOn;
    std::function<void()>playButtonOff;
    std::function<void()>flashPlayButton;
    int xSize = DEFAULT_xSize;
    int ySize = DEFAULT_ySize;
    int currentXstep = xSize;
    int currentYstep = ySize;
    bool playing = false;
    int playStartType = playStartType::immediate;
    uint8_t lastSelected_X = 0;
    uint8_t lastSelected_Y = 0;
    int stepSizeX = 24;
    int stepSizeY = 24;
    enum patternModes { XY_OR, XY_AND, X_SEQ, Y_SEQ, STEP64, X_then_Y };
    bool playingQueued = false;
    bool tick_zero_beat = false;
    bool tick_zero_bar = false;
private:
    std::vector<std::vector<gridElement_t>>grid;
    std::future<void>playNoteResetFuturesArray[1024];
    int playNoteFuturesArrayIndex = 0;
    int xViewIndex = 0;
    int yViewIndex = 0;
    uint32_t tickCount = 0;
    uint32_t tickResetVal = 0 - 1;
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
                [this, i]() {
                    if (this->currentSelectedPattern == i) {
                        otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                        otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 127 : 0;
                        otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 127 : 0;
                    } else {
                        otherColor[15 - i].r = otherColor[15 - i].r == 0 ? 127 : 0;
                        otherColor[15 - i].g = otherColor[15 - i].g == 0 ? 127 : 0;
                        otherColor[15 - i].b = otherColor[15 - i].b == 0 ? 0 : 0;
                    }
                },
                [this, i]() {
                    otherColor[15 - i].r = 127;
                    otherColor[15 - i].g = 127;
                    otherColor[15 - i].b = 127;
                },
                    [this, i]() {
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
    //std::string gridMode = "normal";
    int currentSelectedPattern = 0;
    int numberOfPlayingPatterns = 0;
    std::vector<gridPattern>patterns;
    bool playing = false;
    int mode = gridMode::normal;
}gridState;

struct keysMode_t {
    keysMode_t() {
        for (int i = 0; i < 8; i++)
            for (int u = 0; u < 8; u++)
                this->notes[i][u] = 0;
        this->updateNotes();
    }

    void updateNotes() {
        if (this->layout == layoutType::piano) {
            for (int p = 0; p < 8; p += 2) {
                int iterator = 0;
                for (int i = 0; i < 8; i++) {
                    this->notes[i][p] = scales_lut[1][iterator++] + ((octave + (p / 2)) * 12);
                    if (this->notes[i][p] > 127) this->notes[i][p] = 127;
                    if (this->notes[i][p] < 0) this->notes[i][p] = 0;
                    if (scales_lut[1][iterator] == -1) iterator = 0;
                }
            }
            for (int i = 1; i < 8; i += 2) {
                this->notes[0][i] = 128;
                this->notes[1][i] = ((octave + (i / 2)) * 12) + 1;
                this->notes[2][i] = ((octave + (i / 2)) * 12) + 3;
                this->notes[3][i] = 128;
                this->notes[4][i] = ((octave + (i / 2)) * 12) + 6;
                this->notes[5][i] = ((octave + (i / 2)) * 12) + 8;
                this->notes[6][i] = ((octave + (i / 2)) * 12) + 10;
                this->notes[7][i] = 128;
            }
        }
        else {
            int iterator = 0;
            int iterator2 = 0;
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    this->notes[y][x] = scales_lut[this->layout][iterator++] + ((octave + iterator2) * 12) + this->rootNote;
                    if (scales_lut[this->layout][iterator] == -1 || iterator == 12) {
                        iterator = 0; 
                        iterator2++;
                    }
                }
            }
        }
        
    }

    color_t getNoteColor(int x, int y) {
        color_t temp;
        temp.r = 0;
        temp.g = 0;
        temp.b = 0;

        // check to see if note is root/octave
        int noteVal = this->notes[x][y];
        if (noteVal % 12 == this->rootNote) temp.r = 127;
        else {
            temp.r = 127;
            temp.g = 127;
            temp.b = 127;
        }
        return temp;
    }

    void increaseOctave(){
        if (this->octave >= 8) return;
        this->octave++;
        this->updateNotes();
    }
    void decreaseOctave(){
        if (this->octave <= 0) return;
        this->octave--;
        this->updateNotes();
    }
    void setRootNote(int root){
        if (root > 11 || root < 0) return;
        this->rootNote = root;
        this->updateNotes();
    }
    void nextLayout(){
        if (this->layout >= NUMBER_OF_SCALE_TYPES - 1)return;
        this->layout++;
        this->updateNotes();
    }
    void prevLayout(){
        if (this->layout <= -1) return;
        this->layout--;
        this->updateNotes();
    }

    int octave = 5; // C4 is in the sixth octave above midi note #0
    int rootNote = 0; // C
    enum layoutType{piano = -1, chromatic = 0, major, harmMajor, harmMinor, melodicMinor, 
        wholeTone, majorPent, minorPent, japaneseInSen1, japaneseInSen2, majBebop, dominantBebop, 
        blues, arabic, enigmatic, neapolitan, neapolitanMinor, hungarianMinor, dorian, phrygian, lydian, mixolydian, aeolian, locrian};
    int layout = layoutType::piano;
    const int scales_lut[NUMBER_OF_SCALE_TYPES][12] = {
        {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11}, // chromatic
        {0,  2,  4,  5,  7,  9, 11, -1, -1, -1, -1, -1}, // major
        {0,  2,  4,  5,  7,  8, 11, -1, -1, -1, -1, -1}, // harmonic major
        {0,  2,  3,  5,  7,  8, 11, -1, -1, -1, -1, -1}, // harmonic minor
        {0,  2,  3,  5,  7,  9, 11, -1, -1, -1, -1, -1}, // melodic minor
        {0,  2,  4,  6,  8, 10, -1, -1, -1, -1, -1, -1}, // whole tone
        {0,  2,  4,  7,  9, -1, -1, -1, -1, -1, -1, -1}, // major pentatonic
        {0,  3,  5,  7, 11, -1, -1, -1, -1, -1, -1, -1}, // minor pentatonic
        {0,  1,  5,  7, 11, -1, -1, -1, -1, -1, -1, -1}, // Japanese Insen 1
        {0,  1,  5,  7, 10, -1, -1, -1, -1, -1, -1, -1}, // japanese Insen 2
        {0,  2,  4,  5,  7,  8, 10, -1, -1, -1, -1, -1}, // major bebop
        {0,  2,  4,  5,  7,  9, 10, 11, -1, -1, -1, -1}, // dominant bebop
        {0,  3,  5,  6,  7, 10, -1, -1, -1, -1, -1, -1}, // blues
        {0,  1,  4,  5,  7,  8, 11, -1, -1, -1, -1, -1}, // arabic
        {0,  1,  4,  6,  8, 10, 11, -1, -1, -1, -1, -1}, // enigmatic
        {0,  1,  3,  5,  7,  9, 11, -1, -1, -1, -1, -1}, // neapolitan
        {0,  1,  3,  5,  7,  8, 11, -1, -1, -1, -1, -1}, // neapolitan minor
        {0,  2,  3,  6,  7,  8, 11, -1, -1, -1, -1, -1}, // hungarian Minor
        {0,  2,  3,  5,  7,  9, 10, -1, -1, -1, -1, -1}, // dorian
        {0,  1,  3,  5,  7,  8, 10, -1, -1, -1, -1, -1}, // phrygian
        {0,  2,  4,  6,  7,  9, 11, -1, -1, -1, -1, -1}, // lydian
        {0,  2,  4,  5,  7,  9, 10, -1, -1, -1, -1, -1}, // mixolydian
        {0,  2,  3,  5,  7,  8, 10, -1, -1, -1, -1, -1}, // aeolian
        {0,  1,  3,  5,  6,  8, 10, -1, -1, -1, -1, -1}  // locrian
    };
    int notes[8][8];
}keysMode;

struct xternKb_t {
    int inputDevIndex = -1;
    int lastNoteVal = -1;
    int lastVelocity = -1;
    int lastChord[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    int numKeysDown = 0;
    int velocity = 100;
}midiKeyIn;

RtMidiIn* gridMidiIn = 0; // RTmidi object for LPminiMK3
RtMidiIn* midiIn[128]; // RTmidi objects for all other midi devs
int gridMidiInDevIndex = -1; // indicates which element in midiIn[] will be uninitialized due to it correlating to LPminiMK3
uint16_t numMidiInDevs = 0;
std::string midiInPortNames[128];
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

int main(int argc, char* argv[])
{

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
            midiOut[i] = new RtMidiOut();
        }
        // init grid midi
        gridMidiIn = new RtMidiIn();
        gridMidiOut = new RtMidiOut();
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }


    int tryCount = 0;
    while (!programState.midiDevsInit) {
        tryCount++;
        programState.midiDevsInit = updateMidiDevices();
        std::cout << "Launchpad Mini MK3 not found or MIDI error. Trying again in 1 second.";
        for (int i = 0; i < tryCount; i++)std::cout << ".";
        std::cout << "\r";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\nLaunchpad Mini MK3 found. Continuing." << std::endl;

    if (!loadSettings()) {
        std::cout << "Failed to load settings from file. Continuing with defaults.\n";
    }
    else {
        updateMidiDevices();
    }


    // create a thread to handle all the interprocess stuff
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

    // Start setting up the Launchpad
    setLPtoProgrammerMode();
    if (settings.welcomeMessageEnabled) std::cout << "Welcome to GRID MIDI." << std::endl;
    sendWelcomeMessage();
    copyCurrentPatternGridEnabledToGridColor();

    // init all the variables for the loop
    time_t tempTime = dateNowMicros();
    time_t timerTime = dateNowMicros();
    time_t interval_1ms = 0;
    time_t interval_30ms = 0;
    time_t interval_2s = 0;
    time_t tickTimer_1 = dateNowMillis();
    int logoCycleCount = 0;

    // this loop runs continuously until the user hits a key.
    while (1) {
        //tempTime = ;
        time_t timeDiff = dateNowMicros() - timerTime;
        timerTime = dateNowMicros();
        interval_1ms += timeDiff;
        interval_30ms += timeDiff;
        //interval_2s += timeDiff;

        // every one millisecond
        if (interval_1ms > 1000) {
            interval_1ms = 0;
            time_t tickTimer_2 = dateNowMillis();

            // calculate tick time for the current BPM
            double tickTime = ((60 / (double(gridState.bpm) / 4)) / 96) * 1000;
            if (tickTimer_2 - tickTimer_1 > tickTime) {
                tickTimer_1 = dateNowMillis();
                if (gridState.playing) {
                    // Send midi clock to devices with it enabled
                    for (int i = 0; i < 128; i++) {
                        if (midiOutputDevicesClockEn[i]) {
                            std::vector<unsigned char>message;
                            message.push_back(248);
                            try {
                                midiOut[i]->sendMessage(&message);
                            }
                            catch (RtMidiError& err) {
                                break;
                            }
                        }
                    }
                    // Send Tick to each pattern that is currently playing.
                    for (uint8_t i = 0; i < gridState.patterns.size(); i++) {
                        if (gridState.patterns[i].playing) {
                            gridState.patterns[i].tick();
                        }
                        if (gridState.patterns[0].tick_zero_beat && gridState.patterns[i].playStartType == playStartType::nextBeat) {
                            gridState.patterns[i].playingQueued = false;
                        }
                        if (gridState.patterns[0].tick_zero_bar && gridState.patterns[i].playStartType == playStartType::nextBar) {
                            gridState.patterns[i].playingQueued = false;
                        }
                    }
                }
            }

            // Find noteoff events that are due to be sent and tramsit them
            time_t tempNow = dateNowMillis();
            for (int i = 0; i < 1024; i++) {
                if (playingNotesExpireTimes[i] < tempNow && playingNotesExpireTimes[i] > 0) {
                    playNoteGlobal(currentPlayingNotes[i].portIndex, currentPlayingNotes[i].channel, currentPlayingNotes[i].note, 0, -1);
                    playingNotesExpireTimes[i] = 0;
                }
            }
        }

        // this is about 20 times a second
        if (interval_30ms > 50000) {
            //std::cout << "timediff: " << timeDiff << "\n";
            interval_30ms = 0;
            // Send update to the LPmini3 for all the colors
            switch (gridState.mode) {
            case gridMode::normal:
                copyCurrentPatternGridEnabledToGridColor();
                break;
            case gridMode::drums:
                copyDrumModeColorsToGridColor();
                break;
            case gridMode::keys:
                copyKeysModeColorsToGridColor();
                break;
            case gridMode::user:
                copyUserModeColorsToGridColor();
                break;
            }
            if(sendColors()==-1){
                break;
            }
            // Logo rainbow effect
            double frequency = 0.1;
            gridState.logoColor.r = int(sin(frequency * logoCycleCount + 0) * 63 + 64) | 0;
            gridState.logoColor.g = int(sin(frequency * logoCycleCount + 2) * 63 + 64) | 0;
            gridState.logoColor.b = int(sin(frequency * logoCycleCount + 4) * 63 + 64) | 0;
            logoCycleCount++;
            if (logoCycleCount >= 64)logoCycleCount = 0;
        }

        
        /*if (interval_2s > 2000000) {
            interval_2s = 0;
            //time_t temp1 = dateNowMicros();
            //std::this_thread::sleep_for(std::chrono::microseconds(1));
            //time_t temp2 = dateNowMicros();            
            //std::cout << "diff: " << (temp2 - temp1) << "\n";
        }*/

        if (_kbhit()) {
#ifdef DEBUG_EN
            break;
#endif // DEBUG_EN
            std::string s = "";
            std::cout << "Exit? (y/n): ";
            std::cin >> s;
            if (s == "y")break;
        }
        //std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    for (int i = 0; i < 128; i++) {
        delete midiIn[i];
    }
    delete gridMidiIn;
    for (int i = 0; i < 128; i++) {
        delete midiOut[i];
    }
    delete gridMidiOut;
    if (saveSettings()) {
        std::cout << "Saving global settings.\n";
    }
    else {
        std::cout << "Saving failed.\n";
    }
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
                (LPVOID) hPipe,    // thread parameter 
                0,                 // not suspended 
                &dwThreadId);      // returns thread ID 

            if (hThread == NULL)
            {
                _tprintf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
                return -1;
            } else CloseHandle(hThread);
        } else
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
    TCHAR* pchRequest = (TCHAR*) HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));
    TCHAR* pchReply = (TCHAR*) HeapAlloc(hHeap, 0, BUFSIZE * sizeof(TCHAR));

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
        return (DWORD) -1;
    }

    if (pchRequest == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        return (DWORD) -1;
    }

    if (pchReply == NULL)
    {
        printf("\nERROR - Pipe Server Failure:\n");
        printf("   InstanceThread got an unexpected NULL heap allocation.\n");
        printf("   InstanceThread exitting.\n");
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD) -1;
    }

    // Print verbose messages. In production code, this should be for debugging only.
    //printf("InstanceThread created, receiving and processing messages.\n");

    // The thread's parameter is a handle to a pipe object instance. 

    hPipe = (HANDLE) lpvParam;

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
            } else
            {
                _tprintf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
            }
            break;
        }

        // Process the incoming message.
        ProcessPipeMessage(pchRequest, pchReply, &cbReplyBytes);

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

VOID ProcessPipeMessage(LPTSTR pchRequest, LPTSTR pchReply, LPDWORD pchBytes)
{
    // check that controller is ready
    if (!programState.midiDevsInit || !programState.launchPadInit) {
        *pchBytes = 0;
        pchReply[0] = 0;
        _tprintf(TEXT("Controller not ready (midiDevsInit).\n"));
        return;
    }

    _tprintf(TEXT("Client Request String:\"%s\"\n"), pchRequest);
    ///////////////////////////////////////////////////////////////
    // process IPC string
    std::string stdString_Request;
    std::string reply = "";
    std::wstring w = pchRequest;
    stdString_Request = std::string(w.begin(), w.end()); // magic here
    if (stdString_Request.substr(0, 7) == "isReady") {
        reply = "ready";
        // Check the outgoing message to make sure it's not too long for the buffer.
        if (FAILED(StringCchCopy(pchReply, BUFSIZE, std::wstring(reply.begin(), reply.end()).c_str()))) {
            *pchBytes = 0;
            pchReply[0] = 0;
            printf("StringCchCopy failed, no outgoing message.\n");
            return;
        }
        *pchBytes = (lstrlen(pchReply) + 1) * sizeof(TCHAR);
        return;
    }
    std::string command1 = stdString_Request.substr(0, 3); // first 3 characters are either req or dat for request or data
    std::string command2 = stdString_Request.substr(3, 16); // Next 8 characters are sub commands
    std::string command3 = "";
    if (stdString_Request.size() > 18)
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
        } else if (command2 == "getMidiDevInEn__") {
            reply = "";
            for (int i = 0; i < numMidiInDevs; i++) {
                reply += (midiInputDevicesEnabled[i] ? "1" : "0");
                reply += i == numMidiInDevs - 1 ? "" : ":";
            }
        } else if (command2 == "getKbMidiInDev__") {
            if (midiKeyIn.inputDevIndex != -1) {
                reply = midiInPortNames[midiKeyIn.inputDevIndex];
            } else {
                reply = "Disabled  ";
            }
        } else if (command2 == "getDrumsModeODev") {
            if (drumsModeOutputDevIndex != -1) {
                reply = midiOutPortNames[drumsModeOutputDevIndex];
            } else {
                reply = "Disabled  ";
            }
        } else if (command2 == "getKeysModeODev_") {
            if (keysModeOutputDevIndex != -1) {
                reply = midiOutPortNames[keysModeOutputDevIndex];
            } else {
                reply = "Disabled  ";
            }
        } else if (command2 == "getCurrentGridXY") {
            reply = "";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].lastSelected_X);
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].lastSelected_Y);
        } else if (command2 == "getNoteData_____") {
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
        } else if (command2 == "PatternOptions__") {
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
            reply += ":";
            reply += std::to_string(gridState.patterns[gridState.currentSelectedPattern].playStartType);
        } else if (command2 == "PatternOutPort__") {
            reply = "";
            reply += midiOutPortNames[gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex];
        }
    } else if (command1 == "dat") {
#ifdef DEBUG_EN
        std::cout << "command is data" << std::endl << "command2: " << command2 << std::endl;
#endif // DEBUG_EN
        reply = "Data command recieved.";
        if (command2 == "updateBPM_______") {
            int newBPM = std::stoi(command3);
            gridState.bpm = newBPM;
        } else if (command2 == "selectPattern___") {
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
        } else if (command2 == "setKbInputDev___") {
            for (int i = 0; i < numMidiInDevs; i++) {
                if (midiInPortNames[i].find(command3) != std::string::npos) {
                    midiKeyIn.inputDevIndex = i;
                    updateMidiDevices();
                }
            }
        } else if (command2 == "setDrumOutputDev") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                    drumsModeOutputDevIndex = i;
                }
            }
        } else if (command2 == "setKeysOutputDev") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                    keysModeOutputDevIndex = i;
                }
            }
        } else if (command2 == "setMidiDevInEn__") {
            for (int i = 0; i < numMidiInDevs; i++) {
                if (midiInPortNames[i].find(command3.substr(0, command3.length() - 3)) != std::string::npos) {
                    midiInputDevicesEnabled[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
            updateMidiDevices();
        } else if (command2 == "setMidiOutDevEn_") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3.substr(0, command3.length() - 2)) != std::string::npos) {
                    midiOutputDevicesEnabled[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
            updateMidiDevices();
        } else if (command2 == "setMidiOutClkEn_") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3.substr(0, command3.length() - 2)) != std::string::npos) {
                    midiOutputDevicesClockEn[i] = command3.substr(command3.find(":") + 1) == "1";
                }
            }
        } else if (command2 == "setNoteValue____") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNote(x, y, val, true);
        } else if (command2 == "setNoteVelocity_") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setVelocity(x, y, val, true);
        } else if (command2 == "setNoteLength_ms") {
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int val = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNoteLength(x, y, val, true);
        } else if (command2 == "setNoteLenFrctnl") {
            //std::cout << "command3: " << command3 << std::endl;
            int x = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int y = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int xf = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int yf = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNoteLength(x, y, xf, yf, true);

            //std::cout << "x: " << x << std::endl;
            //std::cout << "y: " << y << std::endl;
            //std::cout << "xf: " << xf << std::endl;
            //std::cout << "yf: " << yf << std::endl;
        } else if (command2 == "patternOptions__") {
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
            int yStepSize = std::stoi(command3.substr(0, command3.find(":")));
            command3 = command3.substr(command3.find(":") + 1);
            int startType = std::stoi(command3);
            gridState.patterns[gridState.currentSelectedPattern].setNumStepsAndSizes(xSteps, ySteps, xStepSize, yStepSize);
            if (chan < 16 && chan >= 0) {
                gridState.patterns[gridState.currentSelectedPattern].setOutPort(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, chan);
            }
            gridState.patterns[gridState.currentSelectedPattern].setMode(mode);
            gridState.patterns[gridState.currentSelectedPattern].playStartType = startType;
        } else if (command2 == "patternMidiDev__") {
            for (int i = 0; i < numMidiOutDevs; i++) {
                if (midiOutPortNames[i].find(command3) != std::string::npos) {
                    gridState.patterns[gridState.currentSelectedPattern].setOutPort(i, gridState.patterns[gridState.currentSelectedPattern].getOutPort().channel);
                }
            }
        } else if (command2 == "LoadFile________") {
            if (loadProject(command3)) {
                reply = "success";
            }
        } else if (command2 == "SaveFile________") {
            if (saveProject(command3)) {
                reply = "success";
            }
        } else {
#ifdef DEBUG_EN
            std::cout << "data command not recognised" << std::endl;
#endif
        }
    } else {
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
    if (FAILED(StringCchCopy(pchReply, BUFSIZE, std::wstring(reply.begin(), reply.end()).c_str())))
    {
        *pchBytes = 0;
        pchReply[0] = 0;
        printf("StringCchCopy failed, no outgoing message.\n");
        return;
    }
    *pchBytes = (lstrlen(pchReply) + 1) * sizeof(TCHAR);
}

void gridMidiInCB(double deltatime, std::vector< unsigned char >* message, void* /*userData*/)
{
    int gridY = (((int) message->at(1) / 10) | 0) - 1;
    int gridX = ((int) message->at(1) % 10) - 1;

    // for any button not on the grid.
    if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
        int patternNum = 0;
        switch ((int)message->at(1)) {
        case 95:
            gridState.mode = gridMode::normal;
            break;
        case 96:
            gridState.mode = gridMode::drums;
            break;
        case 97:
            gridState.mode = gridMode::keys;
            break;
        case 98:
            gridState.mode = gridMode::user;
            break;
        case 89:
            patternNum = 0;
            break;
        case 79:
            patternNum = 1;
            break;
        case 69:
            patternNum = 2;
            break;
        case 59:
            patternNum = 3;
            break;
        case 49:
            patternNum = 4;
            break;
        case 39:
            patternNum = 5;
            break;
        case 29:
            patternNum = 6;
            break;
        case 19:
            // stop, mute, solo button
            patternNum = 7;
            gridState.currentSelectedPattern = 0;
            // stop palying each pattern, reset it, and reset number of playing patterns. This will
            // stop the global playing, tick, and clock output.
            for (uint8_t i = 0; i < gridState.patterns.size(); i++) {
                gridState.patterns[i].playing = false;
                gridState.patterns[i].playingQueued = false;
                gridState.patterns[i].tickReset();
            }
            gridState.numberOfPlayingPatterns = 0;
            break;
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
                    case playStartType::immediate:
                        gridState.patterns[patternNum].playing = !gridState.patterns[patternNum].playing;
                        // set overall play state to true
                        if (gridState.patterns[patternNum].playing) gridState.playing = true;
                        // if pattern was toggled off, reset tick and reduce number of playing patterns
                        if (!gridState.patterns[patternNum].playing) {
                            gridState.patterns[patternNum].tickReset();
                            gridState.numberOfPlayingPatterns--;
                        }
                        else gridState.numberOfPlayingPatterns++; // increase number of playing patterns
                        break;
                    case playStartType::nextBeat:
                    case playStartType::nextBar:
                        gridState.patterns[patternNum].playing = !gridState.patterns[patternNum].playing;
                        if (gridState.patterns[patternNum].playing) gridState.playing = true;

                        if (!gridState.patterns[patternNum].playing) {
                            gridState.patterns[patternNum].tickReset();
                            gridState.numberOfPlayingPatterns--;
                        }
                        else {
                            if (gridState.numberOfPlayingPatterns > 0) {
                                gridState.patterns[patternNum].playingQueued = true;
                            }
                            // increase number of playing patterns
                            gridState.numberOfPlayingPatterns++;
                        }
                        break;
                    }
                }
                else {
                    gridState.patterns[patternNum].playing = !gridState.patterns[patternNum].playing;
                    // set overall play state to true
                    if (gridState.patterns[patternNum].playing) gridState.playing = true; 
                    sendMidiStartStop(gridState.playing);
                    // if pattern was toggled off, reset tick and reduce number of playing patterns
                    if (!gridState.patterns[patternNum].playing) {
                        gridState.patterns[patternNum].tickReset();
                        gridState.numberOfPlayingPatterns--;
                    }
                    else gridState.numberOfPlayingPatterns++; // increase number of playing patterns
                }
                
            }
        }
        if (gridState.numberOfPlayingPatterns == 0) {
            gridState.playing = false;
            sendMidiStartStop(gridState.playing);
        }
    }

    switch (gridState.mode) {
    case gridMode::normal: {
        // if the first byte of the message was 176 and the 3rd was 127, then that was a button down
        if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
            switch ((int)message->at(1)) {
            case 91:
                gridState.patterns[gridState.currentSelectedPattern].increaseYView();
                break;
            case 92:
                gridState.patterns[gridState.currentSelectedPattern].decreaseYView();
                break;
            case 93:
                gridState.patterns[gridState.currentSelectedPattern].decreaseXView();
                break;
            case 94:
                gridState.patterns[gridState.currentSelectedPattern].increaseXView();
                break;
            }
        }
        
        if ((int) message->at(0) == 144 && (int) message->at(2) == 127) { // grid button
            gridState.patterns[gridState.currentSelectedPattern].toggleButton(gridX, gridY);
            if (midiKeyIn.lastNoteVal > -1) {
                gridState.patterns[gridState.currentSelectedPattern].setNote(gridX, gridY, midiKeyIn.lastNoteVal, false);
                gridState.patterns[gridState.currentSelectedPattern].setVelocity(gridX, gridY, midiKeyIn.lastVelocity, false);
            }
        }
        break;
    }
    case gridMode::drums: {
        if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
            switch ((int)message->at(1)) {
            case 91:
                // TODO
                // increase drum dev channel
                //keysMode.increaseOctave();
                break;
            case 92:
                // decrease drum dev channel
                //keysMode.decreaseOctave();
                break;
            }
        }
        if (gridX > 7 || gridY > 7)break;
        //std::cout << (int)message->at(0) << " " << (int)message->at(1) << " " << (int)message->at(2) << "\n";
        
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) { // grid button note on
            playNoteGlobal(drumsModeOutputDevIndex, 0, drumNoteValLUT[gridY][gridX], 100, -1);
        }
        if ((int)message->at(0) == 144 && (int)message->at(2) == 0) { // grid button note off
            playNoteGlobal(drumsModeOutputDevIndex, 0, drumNoteValLUT[gridY][gridX], 0, -1);
        }
        break;
    }
    case gridMode::keys: {
        
        if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
            switch ((int)message->at(1)) {
            case 91:
                // increase octave
                keysMode.increaseOctave();
                break;
            case 92:
                // decrease octave
                keysMode.decreaseOctave();
                break;
            case 93:
                // prev layout
                keysMode.prevLayout();
                break;
            case 94:
                // next layout
                keysMode.nextLayout();
                break;
            }
        }
        if (gridX > 7 || gridY > 7)break;
        if ((int)message->at(0) == 144 && (int)message->at(2) == 127) // grid button note on
            playNoteGlobal(keysModeOutputDevIndex, 0, keysMode.notes[gridX][gridY], 100, -1);
        if ((int)message->at(0) == 144 && (int)message->at(2) == 0) // grid button note off
            playNoteGlobal(keysModeOutputDevIndex, 0, keysMode.notes[gridX][gridY], 0, -1);
        
        break;
    }
    case gridMode::user: {
        if (gridX > 7 || gridY > 7)break;
        if ((int)message->at(0) == 176 && (int)message->at(2) == 127) {
            switch ((int)message->at(1)) {
            case 91:
                //gridState.patterns[gridState.currentSelectedPattern].increaseYView();
                break;
            case 92:
                //gridState.patterns[gridState.currentSelectedPattern].decreaseYView();
                break;
            case 93:
                //gridState.patterns[gridState.currentSelectedPattern].decreaseXView();
                break;
            case 94:
                //gridState.patterns[gridState.currentSelectedPattern].increaseXView();
                break;
            }
        }
        break;
    }
    }
}

/* sets the Launchpad to programmer mode */
void setLPtoProgrammerMode() {
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
    try {
        gridMidiOut->sendMessage(&message);
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
    return;
}

/* Sends welcome message that scolls across the grid. This is a blocking function. */
void sendWelcomeMessage() {
    if (!settings.welcomeMessageEnabled)return;
    std::string welcomeMessage = "GRID MIDI";
    std::string empty = "";
    sendScrollTextToLP(welcomeMessage, 0, 127, 127, 10);
    double frequency = 0.3;
    for (int i = 0; i < 32; i++) {
        int red = int(sin(frequency * i + 0) * 63 + 64) | 0;
        int grn = int(sin(frequency * i + 2) * 63 + 64) | 0;
        int blu = int(sin(frequency * i + 4) * 63 + 64) | 0;
        sendScrollTextToLP(empty, red, grn, blu, 10);
        Sleep(170);
    }
}

void sendScrollTextToLP(std::string& textToSend, uint8_t red, uint8_t grn, uint8_t blu, uint8_t speed) {
    std::vector<unsigned char> message {240,0,32,41,2,13,7,0,speed,1,red,grn,blu};
    for (uint8_t i = 0; i < textToSend.size(); i++) {
        message.push_back(textToSend[i]);
    }
    message.push_back(247);
    try {
        gridMidiOut->sendMessage(&message);
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }
}

time_t dateNowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

time_t dateNowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* sends all of the grid colors to the LP */
int sendColors() {

    std::vector<unsigned char> message {240,0,32,41,2,13,3};
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
    try {
        gridMidiOut->sendMessage(&message);
    }
    catch (RtMidiError& err) {
        return -1;
    }
    std::vector<unsigned char>mes {240,0,32,41,2,13,3,3,99};
    mes.push_back(gridState.logoColor.r);
    mes.push_back(gridState.logoColor.g);
    mes.push_back(gridState.logoColor.b);
    mes.push_back(247);
    try {
        gridMidiOut->sendMessage(&mes);
    }
    catch(RtMidiError& err) {
        return -1;
    }
    return 0;
}

void copyCurrentPatternGridEnabledToGridColor() {
    if (gridState.mode != gridMode::normal)return;
    for (int i = 0; i < 4; i++) {
        gridState.otherColor[i].r = 10;
        gridState.otherColor[i].g = 10;
        gridState.otherColor[i].b = 10;
    }
    for (int i = 4; i < 8; i++) {
        gridState.otherColor[i].r = 0;
        gridState.otherColor[i].g = 0;
        gridState.otherColor[i].b = 0;
    }
    gridState.otherColor[4].r = 127;
    gridState.otherColor[4].g = 127;
    gridState.otherColor[4].b = 127;
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 0;

            if (gridState.patterns[gridState.currentSelectedPattern].getCurrentGridX() == x) {
                gridState.gridColor[x][y].r = 50;
                gridState.gridColor[x][y].g = 50;
                gridState.gridColor[x][y].b = 50;
            }

            if (gridState.patterns[gridState.currentSelectedPattern].getCurrentGridY() == y) {
                gridState.gridColor[x][y].r = 50;
                gridState.gridColor[x][y].g = 50;
                gridState.gridColor[x][y].b = 50;
            }

            if (gridState.patterns[gridState.currentSelectedPattern].getNoteEnabled(x, y, 1)) {
                gridState.gridColor[x][y].b = 127;
                gridState.gridColor[x][y].g = 0;
            }
        }
    }
}

void copyDrumModeColorsToGridColor() {
    if (gridState.mode != gridMode::drums) return;
    for (int i = 0; i < 8; i++) {
        gridState.otherColor[i].r = 0;
        gridState.otherColor[i].g = 0;
        gridState.otherColor[i].b = 0;
    }
    gridState.otherColor[0].r = 10;
    gridState.otherColor[0].g = 10;
    gridState.otherColor[0].b = 10;
    gridState.otherColor[1].r = 10;
    gridState.otherColor[1].g = 10;
    gridState.otherColor[1].b = 10;
    gridState.otherColor[5].r = 127;
    gridState.otherColor[5].g = 127;
    gridState.otherColor[5].b = 127;
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++){
            gridState.gridColor[x][y].r = 127;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 0;
        }
    }
    for (int x = 4; x < 8; x++) {
        for (int y = 0; y < 4; y++) {
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].g = 127;
            gridState.gridColor[x][y].b = 0;
        }
    }
    for (int x = 0; x < 4; x++) {
        for (int y = 4; y < 8; y++) {
            gridState.gridColor[x][y].r = 0;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 127;
        }
    }
    for (int x = 4; x < 8; x++) {
        for (int y = 4; y < 8; y++) {
            gridState.gridColor[x][y].r = 127;
            gridState.gridColor[x][y].g = 0;
            gridState.gridColor[x][y].b = 127;
        }
    }
}

void copyKeysModeColorsToGridColor() {
    if (gridState.mode != gridMode::keys) return;
    for (int i = 0; i < 4; i++) {
        gridState.otherColor[i].r = 10;
        gridState.otherColor[i].g = 10;
        gridState.otherColor[i].b = 10;
    }
    for (int i = 4; i < 8; i++) {
        gridState.otherColor[i].r = 0;
        gridState.otherColor[i].g = 0;
        gridState.otherColor[i].b = 0;
    }
    gridState.otherColor[6].r = 127;
    gridState.otherColor[6].g = 127;
    gridState.otherColor[6].b = 127;

    if (keysMode.layout == keysMode.layoutType::piano) {
        for (int p = 0; p < 8; p += 2) {
            for (int i = 1; i < 7; i++) {
                gridState.gridColor[i][p].r = 127;
                gridState.gridColor[i][p].g = 127;
                gridState.gridColor[i][p].b = 127;
            }
            gridState.gridColor[0][p].r = 127;
            gridState.gridColor[0][p].g = 0;
            gridState.gridColor[0][p].b = 0;
            gridState.gridColor[7][p].r = 127;
            gridState.gridColor[7][p].g = 0;
            gridState.gridColor[7][p].b = 0;
        }
        for (int i = 1; i < 8; i += 2) {
            gridState.gridColor[0][i].r = 0;
            gridState.gridColor[0][i].g = 0;
            gridState.gridColor[0][i].b = 0;
            gridState.gridColor[1][i].r = 0;
            gridState.gridColor[1][i].g = 127;
            gridState.gridColor[1][i].b = 127;
            gridState.gridColor[2][i].r = 0;
            gridState.gridColor[2][i].g = 127;
            gridState.gridColor[2][i].b = 127;
            gridState.gridColor[3][i].r = 0;
            gridState.gridColor[3][i].g = 0;
            gridState.gridColor[3][i].b = 0;
            gridState.gridColor[4][i].r = 0;
            gridState.gridColor[4][i].g = 127;
            gridState.gridColor[4][i].b = 127;
            gridState.gridColor[5][i].r = 0;
            gridState.gridColor[5][i].g = 127;
            gridState.gridColor[5][i].b = 127;
            gridState.gridColor[6][i].r = 0;
            gridState.gridColor[6][i].g = 127;
            gridState.gridColor[6][i].b = 127;
            gridState.gridColor[7][i].r = 0;
            gridState.gridColor[7][i].g = 0;
            gridState.gridColor[7][i].b = 0;
        }
    }
    else {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                gridState.gridColor[x][y].r = keysMode.getNoteColor(x, y).r;
                gridState.gridColor[x][y].g = keysMode.getNoteColor(x, y).g;
                gridState.gridColor[x][y].b = keysMode.getNoteColor(x, y).b;
            }
        }
    }
}

void copyUserModeColorsToGridColor() {
    // TODO: figure out what user mode will do...
    if (gridState.mode != gridMode::user) return;
    for (int i = 0; i < 4; i++) {
        gridState.otherColor[i].r = 10;
        gridState.otherColor[i].g = 10;
        gridState.otherColor[i].b = 10;
    }
    for (int i = 4; i < 8; i++) {
        gridState.otherColor[i].r = 0;
        gridState.otherColor[i].g = 0;
        gridState.otherColor[i].b = 0;
    }
    gridState.otherColor[7].r = 127;
    gridState.otherColor[7].g = 127;
    gridState.otherColor[7].b = 127;
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
    if (note > 127)return;
    // check that this should be sent to midi device
    if (portI < 128 && portI >= 0) {
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
            } else {
                // if velocity is 0, the note should turned off, send note-off
                std::vector<unsigned char>message;
                message.push_back(noteOffMessage | chan);
                message.push_back(note);
                message.push_back(0);
                midiOut[portI]->sendMessage(&message);
            }
        }
    } else if (portI < 1100) {
        // @todo 
        // do CV-gate output
    } else if (portI < 1108) {
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

bool updateMidiDevices() {
    try {
        numMidiInDevs = midiIn[127]->getPortCount();
        if (numMidiInDevs > 128)std::cout << "Only able to handle first 128 midi input devices. " << std::endl;
        std::string portName;
        // go through all available midi devices and look for the LPminiMK3
        for (uint16_t i = 0; i < (numMidiInDevs < 128 ? numMidiInDevs : 128); i++) {
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
            } else {
                if (midiInputDevicesEnabled[i] && !midiIn[i]->isPortOpen()) {
                    midiIn[i]->openPort(i);
                    midiIn[i]->ignoreTypes(false, false, false);
                } else if (!midiInputDevicesEnabled[i] && midiIn[i]->isPortOpen()) {
                    midiIn[i]->closePort();
                }
            }
        }
        if (midiKeyIn.inputDevIndex > -1) {
            if (midiIn[midiKeyIn.inputDevIndex]->isPortOpen()) midiIn[midiKeyIn.inputDevIndex]->setCallback(externKeyboardMidiInputCB);
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
        return false;
    }
    try {
        numMidiOutDevs = midiOut[127]->getPortCount();
        if (numMidiOutDevs > 128)std::cout << "Only able to handle first 128 midi output devices. " << std::endl;
        std::string portName;
        //go through all midi out devices and look for LPminiMK3
        for (uint16_t i = 0; i < (numMidiOutDevs < 128 ? numMidiOutDevs : 128); i++) {
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
            } else {
                if (midiOutputDevicesEnabled[i] && !midiOut[i]->isPortOpen()) {
                    midiOut[i]->openPort(i);
                } else if (!midiOutputDevicesEnabled[i] && midiOut[i]->isPortOpen()) {
                    midiOut[i]->closePort();
                }
            }
            //std::cout << portName << std::endl;
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
        return false;
    }
    if (gridMidiInDevIndex == -1 || gridMidiOutDevIndex == -1) return false;
    return true;
}

void externKeyboardMidiInputCB(double deltatime, std::vector< unsigned char >* message, void* const userData) {
    if ((int) message->at(0) >= 144 && (int) message->at(0) < 160) {
        midiKeyIn.lastChord[midiKeyIn.numKeysDown] = (int)message->at(1);
        midiKeyIn.numKeysDown++;
        midiKeyIn.lastNoteVal = (int) message->at(1);
        midiKeyIn.lastVelocity = (int) message->at(2);
        playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, (int) message->at(0) - 144, (int) message->at(1), (int) message->at(2), -1);
        if (gridState.mode == gridMode::keys) keysMode.setRootNote(((int)message->at(1) % 12));
    }
    if ((int) message->at(0) >= 128 && (int) message->at(0) < 144) {
        midiKeyIn.lastNoteVal = -1;
        midiKeyIn.lastVelocity = -1;
        midiKeyIn.lastChord[midiKeyIn.numKeysDown] = -1;
        if (midiKeyIn.numKeysDown > 0)midiKeyIn.numKeysDown--;
        playNoteGlobal(gridState.patterns[gridState.currentSelectedPattern].getOutPort().portIndex, (int) message->at(0) - 128, (int) message->at(1), 0, 0);
    }
}

bool saveProject(std::string filePath) {
    std::ofstream outFile;
    outFile.open(filePath);
    if (outFile.is_open()) {
        //outFile << "test";
        outFile << "bpm:" << std::to_string(gridState.bpm) << "\n";
        for (int i = 0; i < 7; i++) {
            outFile << "pattern:" << std::to_string(i) << "\n";

            // stepsize x
            outFile << std::to_string(gridState.patterns[i].stepSizeX) << ":";

            // stepsize y
            outFile << std::to_string(gridState.patterns[i].stepSizeY) << ":";

            // num x steps
            outFile << std::to_string(gridState.patterns[i].xSize) << ":";

            // num y steps
            outFile << std::to_string(gridState.patterns[i].ySize) << "\n";

            // pattern mode
            int mode = gridState.patterns[i].getMode();
            outFile << std::to_string(mode) << "\n";

            // pattern start type
            outFile << std::to_string(gridState.patterns[i].playStartType) << "\n";

            // output channel
            // output device string
            outFile << std::to_string(gridState.patterns[i].getOutPort().channel) << ":" << midiOutPortNames[gridState.patterns[i].getOutPort().portIndex] << "\n";

            // all the notes
            for (int x = 0; x < settings.maxXsize; x++) {
                for (int y = 0; y < settings.maxYsize; y++) {
                    gridElement_t temp = gridState.patterns[i].getGridEl(x, y);
                    outFile << std::to_string(temp.enabled) << ":";
                    outFile << std::to_string(temp.note) << ":";
                    outFile << std::to_string(temp.noteLengthFractional_x) << ":";
                    outFile << std::to_string(temp.noteLengthFractional_y) << ":";
                    outFile << (temp.noteLengthUseFractional ? "1:" : "0:");
                    outFile << std::to_string(temp.noteLength_ms) << ":";
                    outFile << std::to_string(temp.velocity) << "\n";
                }
            }
        }
        outFile.close();
    } else {
        std::cout << "Save project failed." << std::endl;
        return false;
    }
    return true;
}

bool loadProject(std::string filePath) {
    std::string line = "";
    std::ifstream inFile;
    inFile.open(filePath);
    if (inFile.is_open()) {
        //outFile << "test";
        //outFile << "bpm:" << std::to_string(gridState.bpm) << "\n";
        std::getline(inFile, line);
        if (line.find("bpm:") == 0) {
            gridState.bpm = std::stoi(line.substr(4));
        } else return false;
        for (int i = 0; i < 7; i++) {
            //outFile << "pattern:" << std::to_string(i) << "\n";
            std::getline(inFile, line);
            if (line.find("pattern:" + std::to_string(i)) != 0) return false;

            std::getline(inFile, line);
            // stepsize x
            gridState.patterns[i].stepSizeX = std::stoi(line.substr(0, line.find(":")));
            line = line.substr(line.find(":") + 1);

            // stepsize y
            gridState.patterns[i].stepSizeY = std::stoi(line.substr(0, line.find(":")));
            line = line.substr(line.find(":") + 1);

            // num x steps
            gridState.patterns[i].xSize = std::stoi(line.substr(0, line.find(":")));
            line = line.substr(line.find(":") + 1);

            // num y steps
            gridState.patterns[i].ySize = std::stoi(line.substr(0, line.find(":")));
            line = line.substr(line.find(":") + 1);

            // pattern mode
            std::getline(inFile, line);
            gridState.patterns[i].setMode(std::stoi(line));


            // pattern start type
            std::getline(inFile, line);
            gridState.patterns[i].playStartType = std::stoi(line);

            // output channel
            // output device string
            std::getline(inFile, line);
            int chan = std::stoi(line.substr(0, line.find(":")));
            line = line.substr(line.find(":") + 1);
            if (line.length() == 0)line = "--------";
            for (int p = 0; p < numMidiOutDevs; p++) {
                if (midiOutPortNames[p].find(line) != std::string::npos) {
                    midiOutputDevicesEnabled[p] = true;
                    gridState.patterns[i].setOutPort(p, chan);
                }
            }
            gridState.patterns[i].setOutPort(gridState.patterns[i].getOutPort().portIndex, chan);
            std::cout << "name: " << midiOutPortNames[gridState.patterns[i].getOutPort().portIndex] << "\n";
            updateMidiDevices();

            // all the notes
            for (int x = 0; x < settings.maxXsize; x++) {
                for (int y = 0; y < settings.maxYsize; y++) {
                    gridElement_t temp;
                    std::getline(inFile, line);

                    temp.enabled = ((line.substr(0, line.find(":")) == "1") ? 1 : 0);
                    line = line.substr(line.find(":") + 1);

                    temp.note = std::stoi(line.substr(0, line.find(":")));
                    line = line.substr(line.find(":") + 1);

                    temp.noteLengthFractional_x = std::stoi(line.substr(0, line.find(":")));
                    line = line.substr(line.find(":") + 1);

                    temp.noteLengthFractional_y = std::stoi(line.substr(0, line.find(":")));
                    line = line.substr(line.find(":") + 1);

                    temp.noteLengthUseFractional = (line.substr(0, line.find(":")) == "1");
                    line = line.substr(line.find(":") + 1);

                    temp.noteLength_ms = std::stoi(line.substr(0, line.find(":")));
                    line = line.substr(line.find(":") + 1);

                    temp.velocity = std::stoi(line.substr(0, line.find(":")));
                    gridState.patterns[i].setGridEl(x, y, temp);
                }
            }
            gridState.patterns[i].playButtonOff();
        }
        gridState.patterns[0].playButtonOn();
        inFile.close();
    } else {
        std::cout << "Load project failed." << std::endl;
        return false;
    }
    return true;
}

bool saveSettings() {
    std::ofstream outFile;
    outFile.open(".\\gridMidi.dat");
    if (outFile.is_open()) {
        // MIDI input devices
        outFile << "midiInDevs;\n";
        for (int i = 0; i < numMidiInDevs; i++) {
            outFile << (midiInputDevicesEnabled[i] ? "1:" : "0:") << midiInPortNames[i] << "\n";
        }
        for (int it = 127; midiInPortNames[it] != "";it--) {
            outFile << (midiInputDevicesEnabled[it] ? "1:" : "0:") << midiInPortNames[it] << "\n";
        }
        // MIDI output devices
        outFile << "midiOutDevs;\n";
        for (int i = 0; i < numMidiOutDevs; i++) {
            outFile << (midiOutputDevicesEnabled[i] ? "1:" : "0:") << (midiOutputDevicesClockEn[i] ? "1:" : "0:") << midiOutPortNames[i] << "\n";
        }
        for (int it = 127; midiOutPortNames[it] != ""; it--) {
            outFile << (midiOutputDevicesEnabled[it] ? "1:" : "0:") << (midiOutputDevicesClockEn[it] ? "1:" : "0:") << midiOutPortNames[it] << "\n";
        }
        outFile << "KBinDev:" << (midiKeyIn.inputDevIndex == -1 ? "disabled" : midiInPortNames[midiKeyIn.inputDevIndex]) << "\n";
        outFile << "DrumsDev:" << (drumsModeOutputDevIndex == -1 ? "disabled" : midiOutPortNames[drumsModeOutputDevIndex]) << "\n";
        outFile << "KeysDev:" << (keysModeOutputDevIndex == -1 ? "disabled" : midiOutPortNames[keysModeOutputDevIndex]) << "\n";
        outFile.close();
    }
    else {
        return false;
    }
    return true;
}

bool loadSettings() {
    std::string line = "";
    std::ifstream inFile;
    inFile.open(".\\gridMidi.dat");
    if (inFile.is_open()) {
        std::getline(inFile, line);
        if (line.find("midiInDevs;") == 0) {
            bool found = false;
            std::getline(inFile, line);
            while (line.find("midiOutDevs;") == std::string::npos) {
                found = false;
                bool temp = stoi(line.substr(0, 1)) == 1;
                line = line.substr(2);
                for (int i = 0; i < numMidiInDevs; i++) {
                    if (midiInPortNames[i].find(line.substr(0, line.length() - 2)) != std::string::npos) {
                        midiInputDevicesEnabled[i] = temp;
                        found = true;
                    }
                }
                if (!found) {
                    int it = 127;
                    while (midiInPortNames[it] != "") it--;
                    midiInPortNames[it] = line;
                    midiInputDevicesEnabled[it] = temp;
                }
                std::getline(inFile, line);
            }
            std::getline(inFile, line);
            while (line.find("KBinDev:") == std::string::npos) {
                found = false;
                bool temp1 = stoi(line.substr(0, 1)) == 1;
                line = line.substr(2);
                bool temp2 = stoi(line.substr(0, 1)) == 1;
                line = line.substr(2);
                for (int i = 0; i < numMidiOutDevs; i++) {
                    if (midiOutPortNames[i].find(line.substr(0, line.length() - 2)) != std::string::npos) {
                        midiOutputDevicesEnabled[i] = temp1;
                        midiOutputDevicesClockEn[i] = temp2;
                        found = true;
                    }
                }
                if (!found) {
                    int it = 127;
                    while (midiOutPortNames[it] != "") it--;
                    midiOutPortNames[it] = line;
                    midiOutputDevicesEnabled[it] = temp1;
                    midiOutputDevicesClockEn[it] = temp2;
                }
                std::getline(inFile, line);
            }
            if (line.find("KBinDev:") != 0) return false;
            line = line.substr(8);
            if (line == "disabled") {
                midiKeyIn.inputDevIndex = -1;
            }
            else {
                for (int i = 0; i < numMidiInDevs; i++) {
                    if (midiInPortNames[i].find(line.substr(0, line.length() - 2)) != std::string::npos) {
                        midiKeyIn.inputDevIndex = i;
                    }
                }
            }
            std::getline(inFile, line);
            if (line.find("DrumsDev:") != 0) return false;
            line = line.substr(9);
            if (line == "disabled") {
                drumsModeOutputDevIndex = -1;
            }
            else {
                for (int i = 0; i < numMidiOutDevs; i++) {
                    if (midiOutPortNames[i].find(line.substr(0, line.length() - 2)) != std::string::npos) {
                        drumsModeOutputDevIndex = i;
                    }
                }
            }
            std::getline(inFile, line);
            if (line.find("KeysDev:") != 0) return false;
            line = line.substr(8);
            if (line == "disabled") {
                keysModeOutputDevIndex = -1;
            }
            else {
                for (int i = 0; i < numMidiOutDevs; i++) {
                    if (midiOutPortNames[i].find(line.substr(0, line.length() - 2)) != std::string::npos) {
                        keysModeOutputDevIndex = i;
                    }
                }
            }
        }
        else return false;
        inFile.close();
    }
    else {
        return false;
    }
    return true;
}

void sendMidiStartStop(bool start) {
    std::vector<unsigned char>message;
    message.push_back(start ? 0xFA : 0xFC);
    for (int i = 0; i < numMidiOutDevs; i++) {
        if(midiOutputDevicesClockEn[i])midiOut[i]->sendMessage(&message);
    }
    
}