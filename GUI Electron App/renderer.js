// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// No Node.js APIs are available in this process because
// `nodeIntegration` is turned off. Use `preload.js` to
// selectively enable features needed in the rendering
// process.

let gridMidi = {}
gridMidi.midiDevices = {}
gridMidi.midiDevices.input = []
gridMidi.midiDevices.output = []
gridMidi.midiDevices.inputDevicesEnabled = []
gridMidi.midiDevices.outputDevicesEnabled = []
gridMidi.midiDevices.outputDevicesClockEnabled = []
gridMidi.midiDevices.numMidiInDevs = 0;
gridMidi.midiDevices.numMidiOutDevs = 0;
gridMidi.midiDevices.midiDevsLoaded = false

gridMidi.lastSelectedGridXY = {}
gridMidi.lastSelectedGridXY.x = 0
gridMidi.lastSelectedGridXY.y = 0
gridMidi.lastSelectedGridXY.noteVal = 0
gridMidi.lastSelectedGridXY.velocity = 0
gridMidi.lastSelectedGridXY.noteLen = 0
gridMidi.lastSelectedGridXY.noteLenFract = [0, 0]
gridMidi.lastSelectedGridXY.isUsingFractionalLen = false

const btn2 = document.getElementById('btn2')
const thingResult = document.getElementById('thingResult')

const launchpadStatus_connectedEl = document.getElementById('launchpadStatus_connected')
const bpm_boxEl = document.getElementById('bpm_box')
const kbInputSelectDropdownEl = document.getElementById('kbInputSelectDropdown')
const drumsModeOutputDeviceSelectorDropdownEl = document.getElementById('drumsModeOutputDeviceSelectorDropdown')
const keysModeOutputDeviceSelectorDropdownEl = document.getElementById('keysModeOutputDeviceSelectorDropdown')
const selectedNoteDisplay_valuesEl = document.getElementById('selectedNoteDisplay_values')
const noteValueDropdownEl = document.getElementById('noteValueDropdown')
const velocityNumberTextEl = document.getElementById('velocityNumberText')
const velocitySliderEl = document.getElementById('velocitySlider')
const noteLength_ms_NumberTextEl = document.getElementById('noteLength_ms_NumberText')
const noteLengthSliderEl = document.getElementById('noteLengthSlider')
const noteLength_fractionalDropdown1El = document.getElementById('noteLength_fractionalDropdown1')
const noteLength_fractionalDropdown2El = document.getElementById('noteLength_fractionalDropdown2')
const patternNum_boxEl = document.getElementById('patternNum_box')
const patternMidiOutDeviceDropdownEl = document.getElementById('patternMidiOutDeviceDropdown')
const patternOutputChannelDropdownEl = document.getElementById('patternOutputChannelDropdown')
const patternModeDropdownEl = document.getElementById('patternModeDropdown')
const xStepsNum_boxEl = document.getElementById('xStepsNum_box')
const yStepsNum_boxEl = document.getElementById('yStepsNum_box')
const xStepSizeNum_boxEl = document.getElementById('xStepSizeNum_box')
const yStepSizeNum_boxEl = document.getElementById('yStepSizeNum_box')
const midiInputDeviceListEl = document.getElementById('midiInputDeviceList')
const midiOutputDeviceListEl = document.getElementById('midiOutputDeviceList')
const fractionalNoteLengthCheckEl = document.getElementById('fractionalNoteLengthCheck')
var getPatternOpts

// Semitones from C to C D E F G A B
const SEMITONES = [0, 2, 4, 5, 7, 9, 11]
// Chromatic melodic scale
const CHROMATIC = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
const NUM = /^\d+(?:\.\d+)?$/


async function checkStatusOfLP() {
    const res = await window.gridMidiAPI.getStatusOfController();
    if (res == 1) {
        launchpadStatus_connectedEl.innerText = "Connected";
        launchpadStatus_connectedEl.style = "color:green";
        return true
    } else {
        launchpadStatus_connectedEl.innerText = "Controller not running";
        launchpadStatus_connectedEl.style = "color:red";
        gridMidi.midiDevices.midiDevsLoaded = false
        return false
    }
}

bpm_boxEl.addEventListener('change', async () => {
    const res = await window.gridMidiAPI.sendMessageToController("datupdateBPM_______" + bpm_boxEl.value)
})

patternNum_boxEl.addEventListener('change', async () => {
    const res = await window.gridMidiAPI.sendMessageToController("datselectPattern___" + (patternNum_boxEl.value - 1))
    getPatternOpts()
})



async function getDataFromController() {
    if (!gridMidi.midiDevices.midiDevsLoaded) {
        let res = await window.gridMidiAPI.sendMessageToController("reqnumMidiDevs_____")
        let res1 = res.substr(0, res.search(';'));
        let res2 = res.substr(res.search(';') + 1)
        gridMidi.midiDevices.numMidiInDevs = parseInt(res1.substr(res1.search(':') + 1, res1.length - 1))
        gridMidi.midiDevices.numMidiOutDevs = parseInt(res2.substr(res2.search(':') + 1, res2.length - 1))
        while (gridMidi.midiDevices.input.length > 0) {
            gridMidi.midiDevices.input.pop()
        }
        while (gridMidi.midiDevices.output.length > 0) {
            gridMidi.midiDevices.output.pop()
        }

        for (let i = 0; i < gridMidi.midiDevices.numMidiInDevs; i++) {
            gridMidi.midiDevices.input.push("")
        }

        res = await window.gridMidiAPI.sendMessageToController("reqmidiInDevNames__")
        let searchIterator = res.search(";")
        let iterator = 0;
        while (1) {
            res1 = res.substr(0, searchIterator - 2)
            gridMidi.midiDevices.input[iterator++] = res1
            res = res.substr(searchIterator + 2)
            searchIterator = res.search(";")
            if (searchIterator == -1) break;
        }

        for (let i = 0; i < gridMidi.midiDevices.numMidiOutDevs; i++) {
            gridMidi.midiDevices.output.push("")
        }

        res = await window.gridMidiAPI.sendMessageToController("reqmidiOutDevNames_")
        searchIterator = res.search(";")
        iterator = 0;
        while (1) {
            res1 = res.substr(0, searchIterator - 2)
            gridMidi.midiDevices.output[iterator++] = res1
            res = res.substr(searchIterator + 2)
            searchIterator = res.search(";")
            if (searchIterator == -1) break;
        }

        res = await window.gridMidiAPI.sendMessageToController("reqgetMidiDevOutEn_")
        for (let i = 0; i < gridMidi.midiDevices.output.length; i++) {
            res1 = res.substr(0, 1)
            res = res.substr(2)
            gridMidi.midiDevices.outputDevicesEnabled.push(res1 == "1")
        }

        res = await window.gridMidiAPI.sendMessageToController("reqgetMOutDevClkEn_")
        for (let i = 0; i < gridMidi.midiDevices.output.length; i++) {
            res1 = res.substr(0, 1)
            res = res.substr(2)
            gridMidi.midiDevices.outputDevicesClockEnabled.push(res1 == "1")
        }

        res = await window.gridMidiAPI.sendMessageToController("reqgetMidiDevInEn__")
        for (let i = 0; i < gridMidi.midiDevices.input.length; i++) {
            res1 = res.substr(0, 1)
            res = res.substr(2)
            gridMidi.midiDevices.inputDevicesEnabled.push(res1 == "1")
        }

        // Setup keyboard input device selection dropdown
        // Setup drums mode output device selection dropdown
        // Setup keys mode output device selection dropdown

        // first we ensure that the html is cleared to a know state
        kbInputSelectDropdownEl.innerHTML = "<option selected disable hidden>-- Please select an option --</option><option>Disabled</option>"
        drumsModeOutputDeviceSelectorDropdownEl.innerHTML = "<option selected disable hidden>-- Please select an option --</option><option>Disabled</option>"
        keysModeOutputDeviceSelectorDropdownEl.innerHTML = "<option selected disable hidden>-- Please select an option --</option><option>Disabled</option>"
        patternMidiOutDeviceDropdownEl.innerHTML = "<option selected disable hidden>-- Please select an option --</option><option>Disabled</option>"

        // then add in all the options for each of the dropdowns
        gridMidi.midiDevices.input.forEach(element => {
            if (element.search("LPMiniMK3") == -1) {
                let newOption = document.createElement("option")
                newOption.innerText = element
                kbInputSelectDropdownEl.appendChild(newOption)
            }
        });

        gridMidi.midiDevices.output.forEach(element => {
            if (element.search("LPMiniMK3") == -1) {
                let newOption = document.createElement("option")
                newOption.innerText = element
                drumsModeOutputDeviceSelectorDropdownEl.appendChild(newOption)

                newOption = document.createElement("option")
                newOption.innerText = element
                keysModeOutputDeviceSelectorDropdownEl.appendChild(newOption)

                newOption = document.createElement("option")
                newOption.innerText = element
                patternMidiOutDeviceDropdownEl.appendChild(newOption)
            }
        });

        kbInputSelectDropdownEl.addEventListener('change', () => {
            // console.log(kbInputSelectDropdownEl.value);
            if (kbInputSelectDropdownEl.value != "-- Please select an option --") {
                window.gridMidiAPI.sendMessageToController("datsetKbInputDev___" + kbInputSelectDropdownEl.value)
            }
        })

        drumsModeOutputDeviceSelectorDropdownEl.addEventListener('change', () => {
            // console.log(drumsModeOutputDeviceSelectorDropdownEl.value);
            if (drumsModeOutputDeviceSelectorDropdownEl.value != "-- Please select an option --") {
                window.gridMidiAPI.sendMessageToController("datsetDrumOutputDev" + drumsModeOutputDeviceSelectorDropdownEl.value)
            }
        })

        keysModeOutputDeviceSelectorDropdownEl.addEventListener('change', () => {
            // console.log(keysModeOutputDeviceSelectorDropdownEl.value);
            if (keysModeOutputDeviceSelectorDropdownEl.value != "-- Please select an option --") {
                window.gridMidiAPI.sendMessageToController("datsetKeysOutputDev" + keysModeOutputDeviceSelectorDropdownEl.value)
            }
        })

        res = await window.gridMidiAPI.sendMessageToController("reqgetKbMidiInDev__")
        let opts = kbInputSelectDropdownEl.children
        for (let i = 0; i < opts.length; i++) {
            if (opts[i].innerText == res.substr(0, res.length - 2)) {
                opts[i].selected = true
            }
        }

        res = await window.gridMidiAPI.sendMessageToController("reqgetDrumsModeODev")
        opts = drumsModeOutputDeviceSelectorDropdownEl.children
        for (let i = 0; i < opts.length; i++) {
            if (opts[i].innerText == res.substr(0, res.length - 2)) {
                opts[i].selected = true
            }
        }

        res = await window.gridMidiAPI.sendMessageToController("reqgetKeysModeODev_")
        opts = keysModeOutputDeviceSelectorDropdownEl.children
        for (let i = 0; i < opts.length; i++) {
            if (opts[i].innerText == res.substr(0, res.length - 2)) {
                opts[i].selected = true
            }
        }

        midiInputDeviceListEl.innerHTML = ""
        midiOutputDeviceListEl.innerHTML = ""

        // build midi in / out configuration
        console.log(gridMidi);
        gridMidi.midiDevices.input.forEach(inputDevice => {
            if (inputDevice.search("LPMiniMK3") == -1) {
                let newMidiDevChild = document.createElement("div")
                let newSpanChild = document.createElement("span")
                newSpanChild.innerHTML = "<div class='midiDeviceName'>" + inputDevice + "</div>"
                //newSpanChild.setAttribute("style","width:100%")
                let newInputChild = document.createElement("input")
                newInputChild.type = "checkbox"
                newInputChild.id = "midiDevEnabledCheck_" + inputDevice

                let index = gridMidi.midiDevices.input.findIndex((element) => {
                    return inputDevice == element
                })
                newInputChild.checked = gridMidi.midiDevices.inputDevicesEnabled[index];

                let newLabelChild = document.createElement("label")
                newLabelChild.innerText = "Enabled"
                newInputChild.addEventListener('change', (element) => {
                    let temp = element.target.checked ? ":1" : ":0"
                    window.gridMidiAPI.sendMessageToController("datsetMidiDevInEn__" + inputDevice + temp)
                })
                newSpanChild.appendChild(newInputChild)
                newSpanChild.appendChild(newLabelChild)
                newMidiDevChild.appendChild(newSpanChild)

                midiInputDeviceListEl.appendChild(newMidiDevChild)
            }
        });

        gridMidi.midiDevices.output.forEach(outputDevice => {
            if (outputDevice.search("LPMiniMK3") == -1) {
                let newMidiDevChild = document.createElement("div")
                let newSpanChild = document.createElement("span")
                newSpanChild.innerHTML = "<div class='midiDeviceName'>" + outputDevice + "</div>"
                let newInputChild = document.createElement("input")
                newInputChild.type = "checkbox"
                newInputChild.id = "midiDevEnabledCheck_" + outputDevice

                let index = gridMidi.midiDevices.output.findIndex((element) => {
                    return outputDevice == element
                })
                newInputChild.checked = gridMidi.midiDevices.outputDevicesEnabled[index];
                newInputChild.addEventListener('change', (element) => {
                    let temp = element.target.checked ? ":1" : ":0"
                    window.gridMidiAPI.sendMessageToController("datsetMidiOutDevEn_" + outputDevice + temp)
                })

                let newLabelChild = document.createElement("label")
                newLabelChild.innerText = "Enabled"
                newSpanChild.appendChild(newInputChild)
                newSpanChild.appendChild(newLabelChild)

                newInputChild = document.createElement("input")
                newInputChild.type = "checkbox"
                newInputChild.id = "midiDevClockEnabledCheck_" + outputDevice

                index = gridMidi.midiDevices.output.findIndex((element) => {
                    return outputDevice == element
                })
                newInputChild.checked = gridMidi.midiDevices.outputDevicesClockEnabled[index];
                newInputChild.addEventListener('change', (element) => {
                    let temp = element.target.checked ? ":1" : ":0"
                    window.gridMidiAPI.sendMessageToController("datsetMidiOutClkEn_" + outputDevice + temp)
                })

                newLabelChild = document.createElement("label")
                newLabelChild.innerText = "Clock Output Enabled"
                newSpanChild.appendChild(newInputChild)
                newSpanChild.appendChild(newLabelChild)

                newMidiDevChild.appendChild(newSpanChild)
                midiOutputDeviceListEl.appendChild(newMidiDevChild)
            }
        });

        res = await window.gridMidiAPI.sendMessageToController('reqPatternOutPort__')
        console.log({res});
        res = res.substr(0,res.length - 2)
        console.log({res});
        patternMidiOutDeviceDropdownEl.value = res

        gridMidi.midiDevices.midiDevsLoaded = true
    }
    res = await window.gridMidiAPI.sendMessageToController("reqgetCurrentGridXY")
    gridMidi.lastSelectedGridXY.x = Number(res.substring(0, res.indexOf(":")))
    gridMidi.lastSelectedGridXY.y = Number(res.substring(res.indexOf(":") + 1))
    selectedNoteDisplay_valuesEl.innerText = "" + String(gridMidi.lastSelectedGridXY.x + 1) + "," + String(gridMidi.lastSelectedGridXY.y + 1)
    res = await window.gridMidiAPI.sendMessageToController("reqgetNoteData_____")
    gridMidi.lastSelectedGridXY.noteVal = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    gridMidi.lastSelectedGridXY.velocity = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    gridMidi.lastSelectedGridXY.noteLen = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    gridMidi.lastSelectedGridXY.noteLenFract[0] = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    gridMidi.lastSelectedGridXY.noteLenFract[1] = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    gridMidi.lastSelectedGridXY.isUsingFractionalLen = res == '1'
    noteValueDropdownEl.value = gridMidi.lastSelectedGridXY.noteVal
    velocitySliderEl.value = gridMidi.lastSelectedGridXY.velocity
    velocityNumberTextEl.innerText = gridMidi.lastSelectedGridXY.velocity
    noteLengthSliderEl.value = gridMidi.lastSelectedGridXY.noteLen
    noteLength_ms_NumberTextEl.innerText = gridMidi.lastSelectedGridXY.noteLen
    noteLength_fractionalDropdown1El.value = gridMidi.lastSelectedGridXY.noteLenFract[1] + 1
    noteLength_fractionalDropdown2El.value = gridMidi.lastSelectedGridXY.noteLenFract[0]
    fractionalNoteLengthCheckEl.checked = gridMidi.lastSelectedGridXY.isUsingFractionalLen
}

patternModeDropdownEl.addEventListener('change', () => {
    console.log(patternModeDropdownEl.value);
})

for (let i = 0; i < 128; i++) {
    let newOption = document.createElement("option")
    newOption.innerText = String(i) + "  -  " + fromMidi(i)
    newOption.value = i
    noteValueDropdownEl.appendChild(newOption)
}

noteValueDropdownEl.addEventListener('input', (event) => {
    window.gridMidiAPI.sendMessageToController("datsetNoteValue____" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(event.target.value))
})

velocitySliderEl.addEventListener('input', (event) => {
    velocityNumberTextEl.innerText = String(event.target.value)
    window.gridMidiAPI.sendMessageToController("datsetNoteVelocity_" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(event.target.value))
})

noteLengthSliderEl.addEventListener('input', (event) => {
    noteLength_ms_NumberTextEl.innerText = String(event.target.value)
    fractionalNoteLengthCheckEl.checked = false
    window.gridMidiAPI.sendMessageToController("datsetNoteLength_ms" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(event.target.value))
})

fractionalNoteLengthCheckEl.addEventListener('change', (event) => {
    if (event.target.checked) {
        window.gridMidiAPI.sendMessageToController("datsetNoteLenFrctnl" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[0]) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[1]))
    }
})

noteLength_fractionalDropdown1El.addEventListener('change', (event) => {
    fractionalNoteLengthCheckEl.checked = true;
    gridMidi.lastSelectedGridXY.noteLenFract[1] = Number(event.target.value) - 1
    window.gridMidiAPI.sendMessageToController("datsetNoteLenFrctnl" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[0]) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[1]))
})

noteLength_fractionalDropdown2El.addEventListener('change', (event) => {
    fractionalNoteLengthCheckEl.checked = true;
    gridMidi.lastSelectedGridXY.noteLenFract[0] = Number(event.target.value)
    window.gridMidiAPI.sendMessageToController("datsetNoteLenFrctnl" + String(gridMidi.lastSelectedGridXY.x) + ":" + String(gridMidi.lastSelectedGridXY.y) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[0]) + ":" + String(gridMidi.lastSelectedGridXY.noteLenFract[1]))
})

xStepsNum_boxEl.addEventListener('input',sendPatternOpts)
yStepsNum_boxEl.addEventListener('input',sendPatternOpts)
yStepSizeNum_boxEl.addEventListener('input',sendPatternOpts)
xStepSizeNum_boxEl.addEventListener('input',sendPatternOpts)
patternModeDropdownEl.addEventListener('input',sendPatternOpts)
patternOutputChannelDropdownEl.addEventListener('input',sendPatternOpts)
patternMidiOutDeviceDropdownEl.addEventListener('input',(event)=>{
    window.gridMidiAPI.sendMessageToController('datpatternMidiDev__' + event.target.value)
})



setInterval(async () => {
    let res = await checkStatusOfLP()
    if (res) {
        // get values from controller and set all stuff on the page
        getDataFromController()
        // console.log(gridMidi);
    }
}, 1000);


(getPatternOpts = async function(){
    let res = await window.gridMidiAPI.sendMessageToController('reqPatternOptions__')
    patternNum_boxEl.value = Number(res.substring(0, res.indexOf(":"))) + 1
    res = res.substring(res.indexOf(":") + 1)
    patternOutputChannelDropdownEl.value = Number(res.substring(0, res.indexOf(":"))) + 1
    res = res.substring(res.indexOf(":") + 1)
    patternModeDropdownEl.value = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    xStepsNum_boxEl.value = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    yStepsNum_boxEl.value = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    xStepSizeNum_boxEl.value = Number(res.substring(0, res.indexOf(":")))
    res = res.substring(res.indexOf(":") + 1)
    yStepSizeNum_boxEl.value = Number(res.substring(0))
    res = await window.gridMidiAPI.sendMessageToController('reqPatternOutPort__')
    res = res.substr(0,res.length - 2)
    patternMidiOutDeviceDropdownEl.value = res
})()

function sendPatternOpts(){
    let mes = "datpatternOptions__"
    mes += String(patternOutputChannelDropdownEl.value - 1)
    mes += ":"
    mes += String(patternModeDropdownEl.value)
    mes += ":"
    mes += String(xStepsNum_boxEl.value)
    mes += ":"
    mes += String(yStepsNum_boxEl.value)
    mes += ":"
    mes += String(xStepSizeNum_boxEl.value)
    mes += ":"
    mes += String(yStepSizeNum_boxEl.value)
    window.gridMidiAPI.sendMessageToController(mes)
}
function fromMidi(midi) {
    var name = CHROMATIC[midi % 12]
    var oct = Math.floor(midi / 12) - 1
    return name + oct
}
function toMidi(p) {
    if (!p[2] && p[2] !== 0) return null
    return SEMITONES[p[0]] + p[1] + 12 * (p[2] + 1)
}
