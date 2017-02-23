# This file hosts tools, config files for cros component API.

## cros_component.config
1. A configuration file in json format specifying what cros components are setup in Chrome OS.
2. An entry in the file includes following info:
    1. name: name of the component; must be a valid non-empty variable.
    2. dir: the name of the dir to install the component (needed by Chrome browser); must be a valid non-empty folder name.
    3. sha2hashstr: 64 long sha256 hash string is generated from a public production key (in omaha server) exclusively for this component.
