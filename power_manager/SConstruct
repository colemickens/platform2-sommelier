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
for key in Split('CFLAGS CPPFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key, '')
  base_env[key] = Split(value)

base_env['CFLAGS'].extend(Split('-Wall -Wextra -Werror -Wuninitialized'))
base_env['CPPFLAGS'].append('-D__STDC_FORMAT_MACROS')

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]

# Build example xidle program
xidle_env = base_env.Clone()
xidle_env.Append(LIBS=Split('base rt X11 Xext'))
xidle_env.Append(CPPPATH=['..'])
xidle_env.ParseConfig('pkg-config --cflags --libs gdk-2.0')
xidle = xidle_env.Library('xidle', ['xidle.cc'])
xidle_example = xidle_env.Program('xidle-example', ['xidle-example.cc', xidle])
Default(xidle_example)

# Build example backlight program
backlight_env = base_env.Clone()
backlight_env.Append(LIBS=Split('base rt gflags'))
backlight_env.Append(CPPPATH=['..'])
backlight = xidle_env.Library('backlight', ['backlight.cc'])
backlight_tool = backlight_env.Program('backlight-tool',
                                       ['backlight-tool.cc', backlight])
Default(backlight_tool)

# Build power daemon
powerd_env = xidle_env.Clone()
powerd_env.Append(LIBS=Split('gflags'))
powerd = powerd_env.Program('powerd', ['powerd.cc', xidle, backlight])
Default(powerd)

# Build unit tests
test_env = xidle_env.Clone()
test_env.Append(LIBS=['gtest', 'Xtst'])
test_env.Program('xidle_unittest',
                 ['testrunner.cc', 'xidle_unittest.cc', xidle])
