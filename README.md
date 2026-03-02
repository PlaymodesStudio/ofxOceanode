ofxOceanode // Macros Rework
=====================================

In this branch we are redoing macros, it's important to know that does not work with current openframeworks bugs, so you need to use [this branch](https://github.com/PlaymodesStudio/openFrameworks/tree/bugfix-ofParameter_change_name).

Oceanode is an openframeworks addon used to quickly create projects using "dataflow like" programming.
The main idea of oceanode became a logic step as we wanted to export our LFO based control to everithing we could imagine.

Introduction
------------
The main idea is to be able to easily create modules and to be able to control it's parameters (with ofParameter) via other modules (modulators). For it's interconnection we use a node based system, that allows us flexibiliy and also easy of view of what is afecting what in realtime.

Another main characteristic is that the connections and nodes layout can be stored as scenes (we call them presets).

License
-------
MIT_Licence

Installation
------------
Just drop the folder into the `openFrameworks/addons/` folder.

Dependencies
------------
[ofxImGuiSimple](https://github.com/PlaymodesStudio/ofxImGuiSimple)

Use
------------
- Add  `#include "ofxOceanode.h"` into `ofApp.h`
- Decalre a ofxOceanode object in `ofApp.h` as:
```cpp
ofxOceanode oceanode;
```

- Setup the object in `ofApp.cpp`:
```cpp
oceanode.setup();
```

- You can add aditional nodes with:
```cpp
oceanode.registerModel<euclideanGenerator>("Generators");
```
with `euclideanGenerator` as the class, and `"Generators"` as the category name.

- For creating custom nodes, check out example-basic.


Compatibility
------------
Only compatible with openFrameworks latest release (0.10) or github master branch.
 - macOs
 - Win
 - Linux
 - Rpi (coming soon)
 - ios (tested but needs some changes to be done to repo)
 
We mainly work on macOs, and we are not testing every change into other platforms. So it's probable that something is broken for those platforms.

Known issues
------------
- Threading done bad, all is done in the draw/update loop, so drawing the gui at 120fps if you want higer data rates it's not optimal.
- Order of execution depends on order of connection created, no way to control them.
- Loading presets is slow.

Version history
------------
### Version 0.1 (03/03/2018):
* First working prototipe
* Basic modulators (phasor, oscillator, oscillator bank, mapper)
* Basic connections for parameters of same type (no min max matching by the moment).
* Connection interaction (create, destroy, move connection when modules moved).
* Module creation and destruction

### Version 0.2 (22/03/2018):
* Infinite Canvas
* More Connections (vector to float, float to vector, void to bool)
* Controls window
* Load save presets (modules, connections, modules params)
* Master BPM control over modules

### Version 0.3 (05/04/2018)
* Upate Phasors and oscillators
* Added methods for saving/loading custom data from nodeModel
* You can define the category of a module, now dropdown for creating new nodes has folders with this categories
* Add custom color to a module (all sliders same color)
* Added external Window node model. Helper class to use nodes that can create a secondary window (for preview or custom guis)
* Parameters can be choosen to be saved on preset or not.
* Added smoother module.

### Version 0.4 (12/04/2018)
* Updated Mapper and Ranger
* Improved connections templates to be able to create more connections (float to vector<int> or vector<float> to vector<int> for example)
* Fix loading connections on preset.
* Be able to update parameters from a nodeModel.
* Json fixes
* Multislider updates.

### Version 0.5 (26/04/2018)
* Added headless mode
* Added BPM detection
* Now we have a types registry to create connections from a custom type. No more need to declare "createConnectionFromCustomType"
* Changed vector<ofEventListener> to ofEventListeners.


### Version 0.6 (19/07/2018)
* Updated remove of Nodes
* Added dulplicate of modules via alt+click on top of a Node
* revised includes to decrease compilation time.
* Implemented Osc communication of parameters.
* Move connections when gui collapse
* Tested in windows
* Implemented setup() method in nodemodel, called when node is created.
* Added functions before loading a preset and after all things have been done when loading a preset.

### Version 0.7 (15/07/2018)
* Oscillator Banks can be modulated with vectors, crazy modulations ahead!
* Dropdown fixes
* Updated OSC.
* Implemeted Persistent Nodes and connections (buggy)
* Implemented Global Phase (Not working)
* Midi Control
* Added randomseed to oscillators/oscillator banks
* string parameters are also saved in preset
* Connection replace instead of new one disapearing
* Fixed phantom connections

### Version 0.8 (14/02/2018)
* Major: Updated preset load connections, now we remove unused connections and connect new ones, before we were destroying all connections
* Windows / Linux compatibility fixes
* New index random modes
    - 0 : noRandom
    - 1 : old method with improved non repeating end (only at -1)
    - -1 : new non repeating method for all the range [0, -1)
* New preset methods, before connecting, and for persistent custom (see future documentation...)
* Phasor new mode, with loop disabled, phasor makes one loop when nreset phase received.
* Phasor now can be multiple, if you connect a vector to beat_div or beat_mult you get a vector of phasors.
* Fixes for changing presets and not having random values apearing in the middle of the preset change.
* Updated examples


Build Status
------------
Linux, macOS [![Build Status](https://travis-ci.org/PlaymodesStudio/ofxOceanode.svg?branch=master)](https://travis-ci.org/PlaymodesStudio/ofxOceanode)
Windows [![Build status](https://ci.appveyor.com/api/projects/status/wwcmfntgs1l5858c/branch/master?svg=true)](https://ci.appveyor.com/project/eduardfrigola/ofxoceanode/branch/master)

