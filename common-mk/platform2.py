#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for building the Chromium OS platform.

Takes care of running gyp/ninja/etc... with all the right values.
"""

from __future__ import print_function

import glob
import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class Platform2(object):
  """Main builder logic for platform2"""

  def __init__(self, use_flags=None, board=None, host=False, libdir=None,
               incremental=True, verbose=False, enable_tests=False,
               cache_dir=None, jobs=None):
    self.board = board
    self.host = host
    self.incremental = incremental
    self.jobs = jobs
    self.verbose = verbose

    if use_flags is not None:
      self.use_flags = use_flags
    else:
      self.use_flags = self.get_platform2_use_flags()

    if enable_tests:
      self.use_flags.add('test')

    if self.host:
      self.sysroot = '/'
      self.pkgconfig = 'pkg-config'
    else:
      board_vars = self.get_portageq_envvars(['SYSROOT', 'PKG_CONFIG'],
                                             board=board)

      self.sysroot = board_vars['SYSROOT']
      self.pkgconfig = board_vars['PKG_CONFIG']

    if libdir:
      self.libdir = libdir
    else:
      self.libdir = '/usr/lib'

    if cache_dir:
      self.cache_dir = cache_dir
    else:
      self.cache_dir = os.path.join(self.sysroot,
                                    'var/cache/portage/chromeos-base/platform2')

  def get_src_dir(self):
    """Return the path to build tools and common GYP files"""
    return os.path.realpath(os.path.dirname(__file__))

  def get_platform2_root(self):
    """Return the path to src/platform2"""
    return os.path.dirname(self.get_src_dir())

  def get_buildroot(self):
    """Return the path to the folder where build artifacts are located."""
    if not self.incremental:
      workdir = os.environ.get('WORKDIR')
      if workdir:
        # Matches $(cros-workon_get_build_dir) behavior.
        return os.path.join(workdir, 'build')
      else:
        return os.getcwd()
    else:
      return self.cache_dir

  def get_products_path(self):
    """Return the path to the folder where build product are located."""
    return os.path.join(self.get_buildroot(), 'out/Default')

  def get_portageq_envvars(self, varnames, board=None):
    """Returns the values of a given set of variables using portageq."""
    if isinstance(varnames, basestring):
      varnames = [varnames]

    # See if the env already has these settings.  If so, grab them directly.
    # This avoids the need to specify --board at all most of the time.
    try:
      board_vars = {}
      for varname in varnames:
        board_vars[varname] = os.environ[varname]
      return board_vars
    except KeyError:
      pass

    if board is None and not self.host:
      board = self.board

    # Portage will set this to an incomplete list which breaks portageq
    # walking all of the repos.  Clear it and let the value be repopulated.
    os.environ.pop('PORTDIR_OVERLAY', None)

    portageq_bin = 'portageq' if not board else 'portageq-%s' % board
    cmd = [portageq_bin, 'envvar', '-v']
    cmd += varnames

    # If a variable isn't found in the config files, portageq will exit 1,
    # so ignore that.  We already hve fallbacks variables.
    ret = cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                                    redirect_stderr=True, error_code_ok=True,
                                    debug_level=logging.DEBUG)
    if ret.returncode > 1 or ret.error:
      raise cros_build_lib.RunCommandError('unknown error', ret)

    output_lines = [x for x in ret.output.splitlines() if x]
    output_items = [x.split('=', 1) for x in output_lines]
    board_vars = dict(dict((k, cros_build_lib.ShellUnquote(v))
                           for k, v in output_items))

    return board_vars if len(board_vars) > 1 else board_vars.values()[0]

  def get_platform2_use_flags(self):
    """Returns the set of USE flags set for the Platform2 package."""
    portageq_bin = 'portageq' if not self.board else 'portageq-%s' % self.board
    cmd = [portageq_bin, 'envvar', 'USE']

    ret = cros_build_lib.RunCommand(cmd, redirect_stdout=True)
    return set(x for x in ret.output.splitlines() if x)

  def get_build_environment(self):
    """Returns a dict containing environment variables we will use to run GYP.

    We do this to set the various CC/CXX/AR names for the target board.
    """
    varnames = ['CHOST', 'AR', 'CC', 'CXX']
    if not self.host and not self.board:
      for v in varnames:
        os.environ.setdefault(v, '')
    board_env = self.get_portageq_envvars(varnames)

    tool_names = {
        'AR': 'ar',
        'CC': 'gcc',
        'CXX': 'g++',
    }

    env = os.environ.copy()
    for var, tool in tool_names.items():
      env['%s_target' % var] = (board_env[var] if board_env[var] else \
                                '%s-%s' % (board_env['CHOST'], tool))

    # ToT GYP uses these environment variables directly when generating ninja
    # files. Remove them from the environment used to run GYP to avoid
    # targets ending up with flags they can't remove. These environment values
    # are passed via external_*flags GYP variables.
    env.pop('CFLAGS', None)
    env.pop('CXXFLAGS', None)
    env.pop('CPPFLAGS', None)
    env.pop('LDFLAGS', None)

    return env

  def get_components_glob(self):
    """Return a glob of marker files for components/projects that were built.

    Each project spits out a file whilst building: we return a glob of them
    so we can install/test those projects or reset between compiles to ensure
    components that are no longer part of the build don't get installed.
    """
    return glob.glob(os.path.join(self.get_products_path(),
                                  'gen/components_*'))

  def use(self, flag):
    """Returns a boolean describing whether or not a given USE flag is set."""
    return flag in self.use_flags

  def configure(self, args):
    """Runs the configure step of the Platform2 build.

    Creates the build root if it doesn't already exists. Generates flags to
    run GYP with, and then runs GYP.
    """
    if not os.path.isdir(self.get_buildroot()):
      os.makedirs(self.get_buildroot())

    if not self.incremental:
      osutils.RmDir(self.get_products_path(), ignore_missing=True)

    targets = [os.path.join(self.get_src_dir(), 'platform.gyp')]
    if args:
      targets = args

    common_gyp = os.path.join(self.get_src_dir(), 'common.gypi')
    libbase_ver = os.environ.get('BASE_VER', '')
    if not libbase_ver:
      # If BASE_VER variable not set, read the content of common_mk/BASE_VER
      # file which contains the default libchrome revision number.
      base_ver_file = os.path.join(self.get_src_dir(), 'BASE_VER')
      libbase_ver = osutils.ReadFile(base_ver_file).strip()

    assert libbase_ver

    # The common root folder of platform2/.
    # Used as (DEPTH) variable in specific project .gyp files.
    src_root = os.path.normpath(os.path.join(self.get_src_dir(), '..'))

    # Do NOT pass the board name into GYP. If you think you need to so, you're
    # probably doing it wrong.
    gyp_args = ['gyp'] + targets + [
        '--format=ninja',
        '--include=%s' % common_gyp,
        '--depth=%s' % src_root,
        '--toplevel-dir=%s' % self.get_platform2_root(),
        '--generator-output=%s' % self.get_buildroot(),
        '-DOS=linux',
        '-Dpkg-config=%s' % self.pkgconfig,
        '-Dsysroot=%s' % self.sysroot,
        '-Dlibdir=%s' % self.libdir,
        '-Dbuild_root=%s' % self.get_buildroot(),
        '-Dplatform2_root=%s' % self.get_platform2_root(),
        '-Dlibbase_ver=%s' % libbase_ver,
        '-Denable_exceptions=%s' % os.environ.get('CXXEXCEPTIONS', '0'),
        '-Dexternal_cflags=%s' % os.environ.get('CFLAGS', ''),
        '-Dexternal_cxxflags=%s' % os.environ.get('CXXFLAGS', ''),
        '-Dexternal_cppflags=%s' % os.environ.get('CPPFLAGS', ''),
        '-Dexternal_ldflags=%s' % os.environ.get('LDFLAGS', ''),
    ]
    # USE flags allow some chars that gyp does not, so normalize them.
    gyp_args += ['-DUSE_%s=1' % (use_flag.replace('-', '_'),)
                 for use_flag in self.use_flags]

    cros_build_lib.RunCommand(gyp_args, env=self.get_build_environment(),
                              cwd=self.get_platform2_root())

  def compile(self, args):
    """Runs the compile step of the Platform2 build.

    Removes any existing component markers that may exist (so we don't run
    tests/install for projects that have been disabled since the last
    build). Builds arguments for running Ninja and then runs Ninja.
    """
    for component in self.get_components_glob():
      os.remove(component)

    if not args:
      args = ['all']
    ninja_args = ['ninja', '-C', self.get_products_path()]
    if self.jobs:
      ninja_args += ['-j', str(self.jobs)]
    ninja_args += args

    if self.verbose:
      ninja_args.append('-v')

    if os.environ.get('NINJA_ARGS'):
      ninja_args.extend(os.environ['NINJA_ARGS'].split())

    cros_build_lib.RunCommand(ninja_args)

  def deviterate(self, args):
    """Runs the configure and compile steps of the Platform2 build.

    This is the default action, to allow easy iterative testing of changes
    as a developer.
    """
    self.configure([])
    self.compile(args)


def GetParser():
  """Return a command line parser."""
  actions = ['configure', 'compile', 'deviterate']

  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--action', default='deviterate',
                      choices=actions, help='action to run')
  parser.add_argument('--board',
                      help='board to build for')
  parser.add_argument('--cache_dir',
                      help='directory to use as cache for incremental build')
  parser.add_argument('--disable_incremental', action='store_false',
                      dest='incremental', help='disable incremental build')
  parser.add_argument('--enable_tests', action='store_true',
                      help='build and run tests')
  parser.add_argument('--host', action='store_true',
                      help='specify that we\'re building for the host')
  parser.add_argument('--libdir',
                      help='the libdir for the specific board, eg /usr/lib64')
  parser.add_argument('--use_flags',
                      action='split_extend', help='USE flags to enable')
  parser.add_argument('-j', '--jobs', type=int, default=None,
                      help='number of jobs to run in parallel')
  parser.add_argument('--verbose', action='store_true', default=None,
                      help='enable verbose log output')
  parser.add_argument('args', nargs='*')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  if options.host and options.board:
    raise AssertionError('You must provide only one of --board or --host')

  if options.verbose is None:
    # Should convert to cros_build_lib.BooleanShellValue.
    options.verbose = (os.environ.get('VERBOSE', '0') == '1')

  p2 = Platform2(options.use_flags, options.board, options.host,
                 options.libdir, options.incremental, options.verbose,
                 options.enable_tests, options.cache_dir, jobs=options.jobs)
  getattr(p2, options.action)(options.args)


if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
