# This file hosts tools, config files for cros component API.

## build_component.py

The script (build_component.py) parses cros_components.json to read
configurations for building all components. For each component to build,
it collect all required binary files, generate final manifest file,
create zip file and upload it to Omaha gs bucket.

## cros_components.json

The configuration file consists of component creation info: package name,
file paths, manifest path, Omaha gs bucket path.
