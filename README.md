ofxOceanode
=====================================

Work in Progress. It is in a usable state. But a lot of things are missing (midi, osc, presets, infinite canvas...).

Introduction
------------
ofxOceanode is an addon to be able to easily create modules and to be able to control it's parameters (with ofParameter) via other modules (modulators). For it's interconnection we use a node based system, that allows us flexibiliy and also easy of view of what is afecting what in realtime.
Also these addon has a preset system, where you can store the combination of modules, parameters and connections. Giving you the abilty to completelly change the scene you are working with.

This addon was developed becouse, when developing [MIRABCN_Generator](https://github.com/PlaymodesStudio/MIRABCN_Generator) we found that the system of modulators and nodes could be used to other projects. We used the node system for other projects, for controlling [ILDA laser](http://www.playmodes.com/home/espills/) and for controlling video buffers and video processors in [ofxPlaymodes](https://github.com/PlaymodesStudio/ofxPlaymodes2017).
Due to the lack of planning, porting the interface to this projects was a pain in the ass, and we want it to be more easy to integrate to new projects and research.
So thats when we realised we had to rebuild all the system to be more flexible, powerfull, expandable, and reusable. Taking some ideas from the opensource node based project [nodeeditor](https://github.com/paceholder/nodeeditor). We have created this addon.

License
-------
MIT_Licence

Installation
------------
Just drop the folder into the `openFrameworks/addons/` folder.

Dependencies
------------
[ofxDatGui_PM/of_master_brach](https://github.com/PlaymodesStudio/ofxDatGui_PM/tree/of_master_branch)

Compatibility
------------
Only compatible woth openFrameworks master branch (of0.10)

Known issues
------------
-

Version history
------------
### Version 0.1 (03/03/2018):
· First working prototipe
· Basic modulators (phasor, oscillator, oscillator bank, mapper)
· Basic connections for parameters of same type (no min max matching by the moment).
· Connection interaction (create, destroy, move connection when modules moved).
· Module creation and destruction


Build Status
------------
Linux, macOS [![Build Status](https://travis-ci.org/PlaymodesStudio/ofxOceanode.svg?branch=master)](https://travis-ci.org/PlaymodesStudio/ofxOceanode)
Windows [![Build status](https://ci.appveyor.com/api/projects/status/wwcmfntgs1l5858c/branch/master?svg=true)](https://ci.appveyor.com/project/eduardfrigola/ofxoceanode/branch/master)

