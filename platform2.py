#!/usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import os
import shutil
import subprocess
import sys


# Define default the version of libchrome{,os} to build against.
# Used only if the BASE_VER environment variable is not set.
# This can also be overridden in a specific target GYP file if required.
_BASE_VER = '180609'


class Platform2(object):
  def __init__(self, use_flags, board=None, host=False, libdir=None,
               incremental=True, verbose=False, enable_tests=False):
    self.board = board
    self.host = host
    self.incremental = incremental
    self.verbose = verbose

    if use_flags:
      self.use_flags = use_flags
    else:
      self.use_flags = self.get_platform2_use_flags()

    if enable_tests:
      self.use_flags.add('test')

    if self.host:
      self.arch = self.get_portageq_envvars('ARCH')
      self.sysroot = '/'
      self.pkgconfig = 'pkg-config'
    else:
      board_vars = self.get_portageq_envvars(['ARCH', 'SYSROOT', 'PKG_CONFIG'],
                                             board=board)

      self.arch = board_vars['ARCH']
      self.sysroot = board_vars['SYSROOT']
      self.pkgconfig = board_vars['PKG_CONFIG']

    if libdir:
      self.libdir = libdir
    else:
      self.libdir = '/usr/lib64' if self.arch == 'amd64' else '/usr/lib'

  def get_src_dir(self):
    """Return the path to build tools and common GYP files"""

    return os.path.realpath(os.path.dirname(__file__))

  def get_platform_root(self):
    """Return the path to src/platform"""

    return os.path.dirname(self.get_src_dir())

  def get_buildroot(self):
    """Return the path to the folder where build artifacts are located."""

    return os.path.join(self.sysroot,
                        'var/cache/portage/chromeos-base/platform2/')

  def get_products_path(self):
    """Return the path to the folder where build product are located."""

    return os.path.join(self.get_buildroot(), 'out/Default')

  def get_portageq_envvars(self, varnames, board = None):
    """Returns the values of a given set of variables using portageq."""
    if isinstance(varnames, basestring):
      varnames = [varnames]

    if board is None and not self.host:
      board = self.board

    portageq_bin = 'portageq' if not board else 'portageq-' + board
    cmd = [portageq_bin, 'envvar', '-v']
    cmd += varnames

    try:
      p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      output, errors = p.communicate()
    except UnboundLocalError:
      raise AssertionError('Error running ' + portageq_bin)
    except OSError:
      raise AssertionError('Error running portageq: ' + errors)

    output_lines = [x for x in output.splitlines() if x]
    output_items = [x.split('=', 1) for x in output_lines]
    board_vars = dict(dict([(k, v[1:-1]) for k, v in output_items]))

    return board_vars if len(board_vars) > 1 else board_vars.values()[0]

  def get_platform2_use_flags(self):
    """Returns the set of USE flags set for the Platform2 package."""
    equery_bin = 'equery' if not self.board else 'equery-' + self.board
    cmd = [equery_bin, 'u', 'platform2']

    try:
      p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
      output, errors = p.communicate()
    except UnboundLocalError:
      raise AssertionError('Error running ' + equery_bin)
    except OSError:
      raise AssertionError('Error running equery: ' + errors)

    return set([x for x in output.splitlines() if x])

  def get_build_environment(self):
    """Returns a dict containing environment variables we will use to run GYP.

    We do this to set the various CC/CXX/AR names for the target board."""

    board_env = self.get_portageq_envvars(['CHOST', 'AR', 'CC', 'CXX'])

    tool_names = {
      'AR': 'ar',
      'CC': 'gcc',
      'CXX': 'g++',
    }

    env = os.environ.copy()
    for var, tool in tool_names.items():
      env[var + '_target'] = board_env[var] if board_env[var] else \
                             board_env['CHOST'] + '-' + tool

    return env

  def get_components_glob(self):
    """Return a glob of marker files for components/projects that were built.

    Each project spits out a file whilst building: we return a glob of them
    so we can install/test those projects or reset between compiles to ensure
    components that are no longer part of the build don't get installed."""

    return glob.glob(os.path.join(self.get_products_path(),
                                  'gen/components_*'))

  def use(self, flag):
    """Returns a boolean describing whether or not a given USE flag is set."""

    return flag in self.use_flags

  def configure(self):
    """Runs the configure step of the Platform2 build.

    Creates the build root if it doesn't already exists. Generates flags to
    run GYP with, and then runs GYP."""

    if not os.path.isdir(self.get_buildroot()):
      os.makedirs(self.get_buildroot())

    if not self.incremental and os.path.isdir(self.get_products_path()):
      shutil.rmtree(self.get_products_path())

    gyp_files = {
      'platform': os.path.join(self.get_src_dir(), 'platform.gyp'),
      'common': os.path.join(self.get_src_dir(), 'common.gypi')
    }

    # Do NOT pass the board name into GYP. If you think you need to so, you're
    # probably doing it wrong.
    gyp_args = [
      'gyp',
      gyp_files['platform'],
      '--format=ninja',
      '--include=' + gyp_files['common'],
      '--depth=' + self.get_platform_root(),
      '--generator-output=' + self.get_buildroot(),
      '-Dpkg-config=' + self.pkgconfig,
      '-Dsysroot=' + self.sysroot,
      '-Dlibdir=' + self.libdir,
      '-Dplatform_root=' + self.get_platform_root(),
      '-Dlibbase_ver=' + os.environ.get('BASE_VER', _BASE_VER),
      '-Dexternal_cflags=' + os.environ.get('CFLAGS', ''),
      '-Dexternal_cxxflags=' + os.environ.get('CXXFLAGS', ''),
      '-Dexternal_ldflags=' + os.environ.get('LDFLAGS', ''),
    ]
    gyp_args += ['-DUSE_%s=1' % (use_flag,) for use_flag in self.use_flags]

    try:
      subprocess.check_call(gyp_args, env=self.get_build_environment(),
                            cwd=self.get_platform_root())
    except subprocess.CalledProcessError:
      raise AssertionError('Error running: %s'
                           % ' '.join(map(repr, gyp_args)))

  def compile(self):
    """Runs the compile step of the Platform2 build.

    Removes any existing component markers that may exist (so we don't run
    tests/install for projects that have been disabled since the last
    build). Builds arguments for running Ninja and then runs Ninja."""

    for component in self.get_components_glob():
      os.remove(component)

    env = os.environ.copy()
    env['PATH'] += ':/mnt/host/depot_tools'

    ninja_args = ['ninja', '-C', self.get_products_path(), 'all']

    if self.verbose:
      ninja_args.append('-v')

    try:
      subprocess.check_call(ninja_args, env=env)
    except subprocess.CalledProcessError:
      raise AssertionError('Error running: %s'
                           % ' '.join(map(repr, ninja_args)))

  def deviterate(self):
    """Runs the configure and compile steps of the Platform2 build.

    This is the default action, to allow easy iterative testing of changes
    as a developer."""

    self.configure()
    self.compile()


class _ParseStringSetAction(argparse.Action):
  def __call__(self, parser, namespace, values, option_string=None):
    setattr(namespace, self.dest, set(values.split()))


def main(argv):
  actions = ['configure', 'compile', 'deviterate']

  parser = argparse.ArgumentParser()
  parser.add_argument('--action', default='deviterate',
                      choices=actions, help='action to run')
  parser.add_argument('--board',
                      help='board to build for')
  parser.add_argument('--disable_incremental', action='store_false',
                      dest='incremental', help='disable incremental build')
  parser.add_argument('--enable_tests', action='store_true',
                      help='build and run tests')
  parser.add_argument('--host', action='store_true',
                      help='specify that we\'re building for the host')
  parser.add_argument('--libdir',
                      help='the libdir for the specific board, eg /usr/lib64')
  parser.add_argument('--use_flags',
                      action=_ParseStringSetAction, help='USE flags to enable')
  parser.add_argument('--verbose', action='store_true', default=None,
                      help='enable verbose log output')

  options = parser.parse_args(argv)

  if not (options.host ^ (options.board != None)):
    raise AssertionError('You must provide only one of --board or --host')

  if options.verbose is None:
    # Should convert to cros_build_lib.BooleanShellValue.
    options.verbose = (os.environ.get('VERBOSE', '0') == '1')

  p2 = Platform2(options.use_flags, options.board, options.host,
                 options.libdir, options.incremental, options.verbose,
                 options.enable_tests)
  getattr(p2, options.action)()


if __name__ == '__main__':
  main(sys.argv[1:])
