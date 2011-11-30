# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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

extra_flags = '-fno-strict-aliasing -Wall -Werror -Wuninitialized ' \
    '-Wnon-virtual-dtor -Woverloaded-virtual'
extra_cppflags = '-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS'
base_env['CFLAGS'].extend(Split(extra_flags))
base_env['CXXFLAGS'].extend(Split(extra_flags))
base_env['CPPFLAGS'].extend(Split(extra_cppflags))

# Switch between new and legacy power button behavior.
# Legacy : power button doesn't send release signal at the right time.
# First button push enables lock screen. Second button push shuts down.
# New : power button supports proper release signal. Long button press to shut
# down.
if ARGUMENTS.get('POWER_BUTTON', '').lower() == 'new':
  base_env.Append(CPPDEFINES=['NEW_POWER_BUTTON'])
if int(ARGUMENTS.get('lockvt', 0)):
  base_env.Append(CPPDEFINES=['SUSPEND_LOCK_VT'])
if int(ARGUMENTS.get('is_desktop', 0)):
  base_env.Append(CPPDEFINES=['IS_DESKTOP'])

# Fix issue with scons not passing some vars through the environment.
for key in Split('SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]
base_env.Append(CPPPATH=['..'])

gdk_env = base_env.Clone()
gdk_env.ParseConfig(os.environ['PKG_CONFIG'] + ' --cflags --libs gdk-2.0')

# Build example xidle program
xidle_env = gdk_env.Clone()
xidle_env.Append(LIBS=Split('base rt X11 pthread Xext'))
xidle = xidle_env.Library('xidle', ['xidle.cc', 'xsync.cc'])
xidle_example = xidle_env.Program('xidle-example',
                                  ['examples/xidle_example.cc', xidle])
Default(xidle_example)

# Build backlight tool
backlight = gdk_env.Library('backlight',
                            ['ambient_light_sensor.cc', 'backlight.cc',
                             'power_constants.cc'])
backlight_env = gdk_env.Clone()
backlight_env.Append(LIBS=Split('base rt pthread gflags'))
backlight_tool = backlight_env.Program('backlight-tool',
                                       ['backlight_tool.cc', backlight])
Default(backlight_tool)

# Build power supply info app
power_supply = gdk_env.Library('power_supply',
                               ['power_supply.cc', 'power_prefs.cc',
                                'inotify.cc'])
power_supply_env = gdk_env.Clone()
power_supply_env.Append(LIBS=Split('base rt pthread gflags'))
power_supply_info = power_supply_env.Program('power-supply-info',
                                             ['power_supply_info.cc',
                                              power_supply])
Default(power_supply_info)

# Build power daemon
backlight_controller = gdk_env.Library('backlight_controller',
                                       ['backlight_controller.cc'])
power_prefs = gdk_env.Library('power_prefs', ['power_prefs.cc',
                                              'power_constants.cc'])
powerd_env = xidle_env.Clone()
powerd_env.Append(LIBS=Split(
    'chromeos gflags pthread dl metrics X11 Xrandr udev'))
powerd_env.ParseConfig(
  os.environ['PKG_CONFIG'] + ' --cflags --libs dbus-1 dbus-glib-1'
)

powerd_sources = Split(''' audio_detector.cc \
  file_tagger.cc power_button_handler.cc powerd.cc powerd_metrics.cc
  screen_locker.cc suspender.cc video_detector.cc metrics_constants.cc
  monitor_reconfigure.cc power_supply.cc resolution_selector.cc
''')
util_lib = powerd_env.Library('util_lib', ['util.cc', 'inotify.cc']);
powerd_lib = powerd_env.Library('powerd_lib', powerd_sources)
powerd = powerd_env.Program('powerd',
                            ['powerd_main.cc', powerd_lib, xidle, backlight,
                             power_prefs, backlight_controller, util_lib])
Default(powerd)

# Build privileged power daemon
powerman_env = gdk_env.Clone()
powerman_env.Append(LIBS=Split('base chromeos gflags pthread metrics X11'))
powerman_env.ParseConfig(
  os.environ['PKG_CONFIG'] + ' --cflags --libs dbus-1 dbus-glib-1'
)
powerman_sources = ['powerman.cc', 'input.cc', 'powerman_metrics.cc']
powerman_lib = powerman_env.Library('powerman_lib', powerman_sources)
powerman = powerman_env.Program('powerm', ['powerman_main.cc', powerman_lib,
                                           power_prefs, util_lib])

Default(powerman)

# Build sample suspend delay client
suspend_delay_sample_env = powerman_env.Clone()
suspend_delay_sample_env.Append(LIBS=Split('base chromeos pthread gflags'))
suspend_delay_sample_env.ParseConfig(
  os.environ['PKG_CONFIG'] + ' --cflags --libs dbus-1 dbus-glib-1'
)
suspend_delay_sample_sources = ['suspend_delay_sample.cc']
suspend_delay_sample = suspend_delay_sample_env.Program('suspend_delay_sample',
                                  suspend_delay_sample_sources)

Default(suspend_delay_sample)


# Build unit tests
tests = []
testrunner = base_env.Library('testrunner', ['testrunner.cc'])
xidle_unittest_env = xidle_env.Clone()
xidle_unittest_env.Append(LIBS=['gtest', 'Xtst'])
deps = [testrunner, 'xidle_unittest.cc', 'mock_xsync.cc', xidle]
tests.append(xidle_unittest_env.Program('xidle_unittest', deps))
backlight_unittest_env = gdk_env.Clone()
backlight_unittest_env.Append(LIBS=Split('base rt gtest pthread gmock Xext'))
deps = [testrunner, 'backlight_unittest.cc', backlight, power_prefs, util_lib]
tests.append(backlight_unittest_env.Program('backlight_unittest', deps))
dimmer_unittest_env = gdk_env.Clone()
dimmer_unittest_env.Append(LIBS=Split('base rt gtest pthread gmock Xext'))
deps = [testrunner, 'idle_dimmer_unittest.cc', backlight_controller, backlight,
        power_prefs, util_lib]
tests.append(dimmer_unittest_env.Program('idle_dimmer_unittest', deps))
deps = [testrunner, 'plug_dimmer_unittest.cc', backlight_controller, backlight,
        power_prefs, util_lib]
tests.append(dimmer_unittest_env.Program('plug_dimmer_unittest', deps))
powerd_unittest_env = powerd_env.Clone()
powerd_unittest_env.Append(LIBS=Split('gtest gmock Xtst'))
deps = [testrunner, 'powerd_unittest.cc', 'backlight_controller_unittest.cc',
        powerd_lib, util_lib, xidle, backlight_controller, backlight,
        power_prefs]
tests.append(powerd_unittest_env.Program('powerd_unittest', deps))
file_tagger_unittest_env = powerd_env.Clone()
file_tagger_unittest_env.Append(LIBS=Split('gtest gmock Xtst'))
deps = [testrunner, 'file_tagger_unittest.cc', powerd_lib,
        util_lib, xidle, backlight_controller, backlight, power_prefs]
tests.append(file_tagger_unittest_env.Program('file_tagger_unittest', deps))
power_supply_unittest_env = powerd_env.Clone()
power_supply_unittest_env.Append(LIBS=Split('gtest gmock Xtst'))
deps = [testrunner, 'power_supply_unittest.cc', powerd_lib, power_prefs,
        util_lib]
tests.append(power_supply_unittest_env.Program('power_supply_unittest', deps))
powerman_unittest_env = powerman_env.Clone()
powerman_unittest_env.Append(LIBS=Split('gtest gmock'))
deps = [testrunner, 'powerman_unittest.cc', powerman_lib, util_lib, power_prefs]
tests.append(powerman_unittest_env.Program('powerman_unittest', deps))
resolution_selector_unittest_env = gdk_env.Clone()
resolution_selector_unittest_env.Append(LIBS=Split(
    'base rt gtest pthread gmock'))
deps = [testrunner, [
        'resolution_selector_unittest.cc', 'resolution_selector.cc'],
        backlight_controller, backlight, power_prefs]
tests.append(resolution_selector_unittest_env.Program(
    'resolution_selector_unittest', deps))
AlwaysBuild(base_env.Alias('tests', tests))
