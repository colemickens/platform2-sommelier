# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

Help("""\
Type: 'scons' to build and 'scons -c' to clean\
""")

# Create a base environment including things that are likely to be common
# to all of the objects in this directory. We pull in overrides from the
# environment to enable cross-compile.
base_env = Environment()
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value is not None:
    base_env[key] = value
for key in Split('CFLAGS CPPFLAGS CXXFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key, '')
  base_env[key] = Split(value)

extra_flags = '-fno-strict-aliasing -Wall -Wextra -Werror -Wuninitialized'
base_env['CFLAGS'].extend(Split(extra_flags))
base_env['CXXFLAGS'].extend(Split(extra_flags))
base_env['CPPFLAGS'].append('-D__STDC_FORMAT_MACROS')

# Fix issue with scons not passing some vars through the environment.
for key in Split('SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]
base_env.Append(CPPPATH=['..'])

gdk_env = base_env.Clone()
gdk_env.ParseConfig(os.environ['PKG_CONFIG'] + ' --cflags --libs gdk-2.0')

# Build example xidle program
xidle_env = gdk_env.Clone()
xidle_env.Append(LIBS=Split('base rt X11 Xext'))
xidle = xidle_env.Library('xidle', ['xidle.cc'])
xidle_example = xidle_env.Program('xidle-example', ['xidle-example.cc', xidle])
Default(xidle_example)

# Build backlight tool
backlight = gdk_env.Library('backlight', ['backlight.cc'])
backlight_env = gdk_env.Clone()
backlight_env.Append(LIBS=Split('base rt gflags'))
backlight_tool = backlight_env.Program('backlight-tool',
                                       ['backlight-tool.cc', backlight])
Default(backlight_tool)

# Build power daemon
backlight_controller = base_env.Library('backlight_controller',
                                        ['backlight_controller.cc'])
power_prefs = base_env.Library('power_prefs', ['power_prefs.cc'])
powerd_env = xidle_env.Clone()
powerd_env.Append(LIBS=Split('gflags metrics'))
powerd_lib = powerd_env.Library('powerd_lib',
                                ['powerd.cc', 'powerd_metrics.cc'])
load_lib = base_env.Library('load',  ['../cros/load.cc'])
powerd = powerd_env.Program('powerd',
                            ['powerd_main.cc', powerd_lib, xidle, backlight,
                             power_prefs, backlight_controller, load_lib])
Default(powerd)

# Build unit tests
tests = []
testrunner = base_env.Library('testrunner', ['testrunner.cc'])
xidle_unittest_env = xidle_env.Clone()
xidle_unittest_env.Append(LIBS=['gtest', 'Xtst'])
deps = [testrunner, 'xidle_unittest.cc', xidle]
tests.append(xidle_unittest_env.Program('xidle_unittest', deps))
dimmer_unittest_env = gdk_env.Clone()
dimmer_unittest_env.Append(LIBS=Split('base rt gtest gmock'))
deps = [testrunner, 'idle_dimmer_unittest.cc', backlight_controller,
        power_prefs]
tests.append(dimmer_unittest_env.Program('idle_dimmer_unittest', deps))
deps = [testrunner, 'plug_dimmer_unittest.cc', backlight_controller,
        power_prefs]
tests.append(dimmer_unittest_env.Program('plug_dimmer_unittest', deps))
powerd_unittest_env = xidle_env.Clone()
powerd_unittest_env.Append(LIBS=Split('base rt gtest gmock'))
deps = [testrunner, 'powerd_unittest.cc', powerd_lib, xidle,
        backlight_controller, power_prefs, load_lib]
tests.append(dimmer_unittest_env.Program('powerd_unittest', deps))
AlwaysBuild(base_env.Alias('tests', tests))
