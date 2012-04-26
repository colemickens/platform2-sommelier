# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

Help("""\
Type: 'scons' to build and 'scons -c' to clean\
""")

# Borrowed from updater
# Protobuffer compilation
def ProtocolBufferEmitter(target, source, env):
  """ Inputs:
          target: list of targets to compile to
          source: list of sources to compile
          env: the scons environment in which we are compiling
      Outputs:
          target: the list of targets we'll emit
          source: the list of sources we'll compile"""
  output = str(source[0])
  output = output[0:output.rfind('.proto')]
  target = [
    output + '.pb.cc',
    output + '.pb.h',
  ]
  return target, source

def ProtocolBufferGenerator(source, target, env, for_signature):
  """ Inputs:
          source: list of sources to process
          target: list of targets to generate
          env: scons environment in which we are working
          for_signature: unused
      Outputs: a list of commands to execute to generate the targets from
               the sources."""
  commands = [
    '/usr/bin/protoc '
    ' --proto_path=. ${SOURCES} --cpp_out=.']
  return commands

proto_builder = Builder(generator = ProtocolBufferGenerator,
                        emitter = ProtocolBufferEmitter,
                        single_source = 1,
                        suffix = '.pb.cc')

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
if int(ARGUMENTS.get('new_power_button', 0)):
  base_env.Append(CPPDEFINES=['NEW_POWER_BUTTON'])
if int(ARGUMENTS.get('lockvt', 0)):
  base_env.Append(CPPDEFINES=['SUSPEND_LOCK_VT'])
if int(ARGUMENTS.get('is_desktop', 0)):
  base_env.Append(CPPDEFINES=['IS_DESKTOP'])
if int(ARGUMENTS.get('has_als', 0)):
  base_env.Append(CPPDEFINES=['HAS_ALS'])
if int(ARGUMENTS.get('has_keyboard_backlight', 0)):
  base_env.Append(CPPDEFINES=['HAS_KEYBOARD_BACKLIGHT'])

# Fix issue with scons not passing some vars through the environment.
for key in Split('SYSROOT'):
  if key in os.environ:
    base_env['ENV'][key] = os.environ[key]
base_env.Append(CPPPATH=['..'])

BASE_VER = os.environ.get('BASE_VER', '125070')
pkgconfig = os.environ.get('PKG_CONFIG', 'pkg-config')
gdk_env = base_env.Clone()
gdk_env.ParseConfig('%s --cflags --libs gdk-2.0 libchrome-%s libchromeos-%s'
                    % (pkgconfig, BASE_VER, BASE_VER))

# Build example xidle program
xidle_env = gdk_env.Clone()
# WARNING! Dear person who thinks these library dependencies should be in
# the alphabetical order. No, you are wrong. Do not attempt to change the
# order without copious testing. In particular, the 'chromeos' dependency
# should appear before 'base' or bad things happen.
xidle_env.Append(LIBS=Split('X11 Xext'))
xidle = xidle_env.Library('xidle', ['xidle.cc', 'xsync.cc'])
xidle_example = xidle_env.Program('xidle-example',
                                  ['examples/xidle_example.cc', xidle])
Default(xidle_example)

# Build backlight tool
backlight_env = gdk_env.Clone()
backlight_env.Append(LIBS=Split('gflags metrics X11 Xrandr udev'))
backlight_env.ParseConfig('%s --cflags --libs dbus-1 dbus-glib-1 libchromeos-%s'
                          % (pkgconfig, BASE_VER))
backlight = backlight_env.Library('backlight',
                                  ['ambient_light_sensor.cc', 'backlight.cc',
                                   'external_backlight.cc',
                                   'external_backlight_client.cc',
                                   'power_constants.cc'])
backlight_tool = backlight_env.Program('backlight-tool',
                                       ['backlight_tool.cc', backlight])
Default(backlight_tool)

# Build power supply info app
power_supply = gdk_env.Library('power_supply',
                               ['power_supply.cc', 'power_prefs.cc',
                                'inotify.cc'])
power_supply_env = gdk_env.Clone()
power_supply_env.Append(LIBS=Split('gflags'))
power_supply_info = power_supply_env.Program('power-supply-info',
                                             ['power_supply_info.cc',
                                              power_supply])
Default(power_supply_info)

# Build power daemon
backlight_controller = gdk_env.Library('backlight_controller',
                                       ['internal_backlight_controller.cc'])
power_prefs = gdk_env.Library('power_prefs', ['power_prefs.cc',
                                              'power_constants.cc'])
powerd_env = gdk_env.Clone()
powerd_env.Command('./power_supply_properties.proto',
                   os.environ['SYSROOT'] +
                   '/usr/include/chromeos/dbus/power_supply_properties.proto',
                   Copy('$TARGET', '$SOURCE'))
powerd_env.Command('./power_state_control.proto',
                   os.environ['SYSROOT'] +
                   '/usr/include/chromeos/dbus/power_state_control.proto',
                   Copy('$TARGET', '$SOURCE'))
powerd_env['BUILDERS']['ProtocolBuffer'] = proto_builder
powerd_env.Append(LIBS=Split('Xext gflags metrics X11 Xrandr udev protobuf'))
powerd_env.ParseConfig('%s --cflags --libs dbus-1 dbus-glib-1 libchromeos-%s'
                       % (pkgconfig, BASE_VER))
powerd_env.ProtocolBuffer('power_supply_properties.pb.cc',
                          'power_supply_properties.proto')
powerd_env.ProtocolBuffer('power_state_control.pb.cc',
                          'power_state_control.proto')
powerd_env.Depends('powerd.cc',
                   'power_supply_properties.pb.cc')
powerd_env.Depends('state_control.cc',
                   'power_state_control.pb.cc')
powerd_sources = Split('''audio_detector.cc file_tagger.cc metrics_constants.cc
                       metrics_store.cc monitor_reconfigure.cc
                       power_state_control.pb.cc power_supply.cc
                       power_supply_properties.pb.cc powerd.cc
                       powerd_metrics.cc resolution_selector.cc
                       rolling_average.cc screen_locker.cc state_control.cc
                       suspender.cc video_detector.cc ''')
util_lib = powerd_env.Library('util_lib', ['util.cc', 'inotify.cc']);
powerd_lib = powerd_env.Library('powerd_lib', powerd_sources)
powerd = powerd_env.Program('powerd',
                            ['powerd_main.cc', powerd_lib, xidle, backlight,
                             power_prefs, backlight_controller, util_lib])
Default(powerd)

# Build privileged power daemon
powerman_env = gdk_env.Clone()
powerman_env.Append(LIBS=Split('gflags metrics udev X11'))
powerman_env.ParseConfig('%s --cflags --libs dbus-1 dbus-glib-1 libchromeos-%s'
                         % (pkgconfig, BASE_VER))
powerman_sources = ['powerman.cc', 'external_backlight.cc',
                    'input.cc', 'powerman_metrics.cc']
powerman_lib = powerman_env.Library('powerman_lib', powerman_sources)
powerman = powerman_env.Program('powerm', ['powerman_main.cc', powerman_lib,
                                           power_prefs, util_lib])

Default(powerman)

# Build sample suspend delay client
suspend_delay_sample_env = powerman_env.Clone()
suspend_delay_sample_env.Append(LIBS=Split('gflags'))
suspend_delay_sample_env.ParseConfig(('%s --cflags --libs dbus-1 dbus-glib-1 ' +
                                      'libchromeos-%s') % (pkgconfig, BASE_VER))
suspend_delay_sample_sources = ['suspend_delay_sample.cc']
suspend_delay_sample = suspend_delay_sample_env.Program('suspend_delay_sample',
                                  suspend_delay_sample_sources)

Default(suspend_delay_sample)

# Build power state control tool
power_state_tool_env = gdk_env.Clone()
power_state_tool_env.Depends('power_state_tool.cc',
                             'power_state_control.pb.cc')
power_state_tool_env.Append(LIBS=Split('rt pthread gflags protobuf X11'))
power_state_tool_env.ParseConfig(('%s --cflags --libs dbus-1 dbus-glib-1 ' +
                                  'libchromeos-%s') % (pkgconfig, BASE_VER))
power_state_tool = power_state_tool_env.Program('power_state_tool',
                                                ['power_state_tool.cc',
                                                 'power_state_control.pb.cc',
                                                 'power_constants.cc',
                                                 'util.cc'])
Default(power_state_tool)

# Build backlight dbus tool
backlight_dbus_tool = gdk_env.Clone()
backlight_dbus_tool.Append(LIBS=Split('rt pthread gflags protobuf X11'))
backlight_dbus_tool.ParseConfig(('%s --cflags --libs dbus-1 dbus-glib-1 ' +
                                 'libchromeos-%s') % (pkgconfig, BASE_VER))
backlight_dbus_tool = backlight_dbus_tool.Program('backlight_dbus_tool',
                                                  ['backlight_dbus_tool.cc',
                                                   'power_constants.cc',
                                                   'util.cc'])
Default(backlight_dbus_tool)

# Build unit tests
tests = []
testrunner = base_env.Library('testrunner', ['testrunner.cc'])
xidle_unittest_env = xidle_env.Clone()
xidle_unittest_env.Append(LIBS=['gtest', 'Xtst'])
deps = [testrunner, 'xidle_unittest.cc', 'mock_xsync.cc', xidle]
tests.append(xidle_unittest_env.Program('xidle_unittest', deps))
backlight_unittest_env = gdk_env.Clone()
backlight_unittest_env.Append(LIBS=Split('gtest gmock Xext'))
deps = [testrunner, 'backlight_unittest.cc', backlight, power_prefs, util_lib]
tests.append(backlight_unittest_env.Program('backlight_unittest', deps))
dimmer_unittest_env = gdk_env.Clone()
dimmer_libs = Split('gtest gmock Xext X11 Xrandr');
dimmer_unittest_env.Append(LIBS=dimmer_libs);
deps = [testrunner, 'idle_dimmer_unittest.cc', 'monitor_reconfigure.cc',
        'resolution_selector.cc', backlight_controller, backlight, power_prefs,
        util_lib]
tests.append(dimmer_unittest_env.Program('idle_dimmer_unittest', deps))
deps = [testrunner, 'plug_dimmer_unittest.cc', 'monitor_reconfigure.cc',
        'resolution_selector.cc', backlight_controller, backlight, power_prefs,
        util_lib]
tests.append(dimmer_unittest_env.Program('plug_dimmer_unittest', deps))
powerd_unittest_env = powerd_env.Clone()
powerd_unittest_env.Append(LIBS=Split('gtest gmock Xtst'))
deps = [testrunner, 'powerd_unittest.cc',
        'internal_backlight_controller_unittest.cc',
        'metrics_store_unittest.cc', 'rolling_average_unittest.cc',
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
resolution_selector_unittest_env.Append(LIBS=Split('gtest gmock'))
deps = [testrunner, [
        'resolution_selector_unittest.cc', 'resolution_selector.cc'],
        backlight_controller, backlight, power_prefs]
tests.append(resolution_selector_unittest_env.Program(
    'resolution_selector_unittest', deps))

state_control_unittest_env = powerd_env.Clone()
state_control_unittest_env.Append(LIBS=Split('gtest gmock Xtst'))
deps = [testrunner, 'state_control_unittest.cc', 'state_control.cc',
        powerd_lib, util_lib, xidle,
        backlight_controller, backlight, power_prefs]
tests.append(state_control_unittest_env.Program('state_control_unittest', deps))

AlwaysBuild(base_env.Alias('tests', tests))
