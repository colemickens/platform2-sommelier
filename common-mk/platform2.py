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
from chromite.lib import osutils
from chromite.lib import portage_util

import common_utils

# List of all USE flags that can be used in BUILD.gn.
_IUSE = [
    'amd64',
    'arm',
    'asan',
    'attestation',
    'binder',
    'buffet',
    'cellular',
    'cert_provision',
    'cfm_enabled_device',
    'cheets',
    'chromeless_tty',
    'containers',
    'coverage',
    'cros_host',
    'crosvm_wl_dmabuf',
    'crypto',
    'cryptohome_userdataauth_interface',
    'dbus',
    'device_mapper',
    'dhcpv6',
    'direncryption',
    'dlc',
    'feedback',
    'ftdi_tpm',
    'fuzzer',
    'hammerd_api',
    'iwlwifi_dump',
    'kvm_host',
    'metrics_uploader',
    'mojo',
    'msan',
    'mtd',
    'opengles',
    'passive_metrics',
    'pinweaver',
    'power_management',
    'pppoe',
    'profiling',
    'selinux',
    'systemd',
    'tcmalloc',
    'test',
    'timers',
    'tpm',
    'tpm2',
    'tpm2_simulator',
    'ubsan',
    'vpn',
    'wake_on_wifi',
    'wifi',
    'wired_8021x',
]


class Platform2(object):
  """Main builder logic for platform2"""

  def __init__(self, use_flags=None, board=None, host=False, libdir=None,
               incremental=True, verbose=False, enable_tests=False,
               cache_dir=None, jobs=None, platform_subdir=None):
    self.board = board
    self.host = host
    self.incremental = incremental
    self.jobs = jobs
    self.verbose = verbose
    self.platform_subdir = platform_subdir

    if use_flags is not None:
      self.use_flags = use_flags
    else:
      self.use_flags = portage_util.GetBoardUseFlags(self.board)

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

    self.libbase_ver = os.environ.get('BASE_VER', '')
    if not self.libbase_ver:
      # If BASE_VER variable not set, read the content of common_mk/BASE_VER
      # file which contains the default libchrome revision number.
      base_ver_file = os.path.join(self.get_src_dir(), 'BASE_VER')
      self.libbase_ver = osutils.ReadFile(base_ver_file).strip()
    assert self.libbase_ver


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

    return portage_util.PortageqEnvvars(varnames, board=board,
                                        allow_undefined=True)

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

  def should_use_gn(self):
    """Returns true if GN should be used on configure."""
    build_gn = os.path.join(self.get_platform2_root(), self.platform_subdir,
                            'BUILD.gn')
    return os.path.isfile(build_gn)

  def configure(self, args):
    """Runs the configure step of the Platform2 build.

    Creates the build root if it doesn't already exists.  Then runs the
    appropriate configure tool (e.g. GYP, GN, etc...).
    """
    if not os.path.isdir(self.get_buildroot()):
      os.makedirs(self.get_buildroot())

    if not self.incremental:
      osutils.RmDir(self.get_products_path(), ignore_missing=True)

    if self.should_use_gn():
      # The args is only for gyp.
      assert len(args) == 0 or (len(args) == 1 and '.gyp' in args[0])
      self.configure_gn()
    else:
      self.configure_gyp(args)

  def gen_common_args(self, should_parse_shell_string):
    """Generates common arguments for the tools to configure as a dict.

    Returned value types are str, bool or list of strs.
    Lists are returned only when should_parse_shell_string is set to True.
    """
    def flags(s):
      if should_parse_shell_string:
        return common_utils.parse_shell_args(s)
      return s

    args = {
        'OS': 'linux',
        'pkg-config': self.pkgconfig,
        'sysroot': self.sysroot,
        'libdir': self.libdir,
        'build_root': self.get_buildroot(),
        'platform2_root': self.get_platform2_root(),
        'libbase_ver': self.libbase_ver,
        'enable_exceptions': os.environ.get('CXXEXCEPTIONS', 0) == '1',
        'external_cflags': flags(os.environ.get('CFLAGS', '')),
        'external_cxxflags': flags(os.environ.get('CXXFLAGS', '')),
        'external_cppflags': flags(os.environ.get('CPPFLAGS', '')),
        'external_ldflags': flags(os.environ.get('LDFLAGS', '')),
    }
    return args

  def configure_gyp(self, args):
    """Configure with GYP.

    Generates flags to run GYP with, and then runs GYP.
    """
    targets = [os.path.join(self.get_src_dir(), 'platform.gyp')]
    if args:
      targets = args

    common_gyp = os.path.join(self.get_src_dir(), 'common.gypi')

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
    ]
    common_args = self.gen_common_args(False)
    for k, v in common_args.iteritems():
      if isinstance(v, bool):
        v = int(v)
      gyp_args.append('-D%s=%s' % (k, v))

    # USE flags allow some chars that gyp does not, so normalize them.
    gyp_args += ['-DUSE_%s=1' % (use_flag.replace('-', '_'),)
                 for use_flag in self.use_flags]

    cros_build_lib.RunCommand(gyp_args, env=self.get_build_environment(),
                              cwd=self.get_platform2_root())

  def configure_gn(self):
    """Configure with GN.

    Generates flags to run GN with, and then runs GN.
    """
    def to_gn_string(s):
      return '"%s"' % s.replace('"', '\\"')

    def to_gn_list(strs):
      return '[%s]' % ','.join([to_gn_string(s) for s in strs])

    def to_gn_args_args(gn_args):
      for k, v in gn_args.items():
        if isinstance(v, bool):
          v = str(v).lower()
        elif isinstance(v, list):
          v = to_gn_list(v)
        elif isinstance(v, str):
          v = to_gn_string(v)
        else:
          raise AssertionError('Unexpected value type %s' % str(type(v)))
        yield '%s=%s' % (k.replace('-', '_'), v)

    buildenv = self.get_build_environment()
    gn_args = {
        'platform_subdir': self.platform_subdir,
        'cc': buildenv.get('CC_target', buildenv.get('CC', '')),
        'cxx': buildenv.get('CXX_target', buildenv.get('CXX', '')),
        'ar': buildenv.get('AR_target', buildenv.get('AR', '')),
    }
    gn_args['clang_cc'] = 'clang' in gn_args['cc']
    gn_args['clang_cxx'] = 'clang' in gn_args['cxx']
    gn_args.update(self.gen_common_args(True))
    gn_args_args = list(to_gn_args_args(gn_args))

    # Set use flags as a scope.
    uses = {}
    for flag in _IUSE:
      uses[flag] = False
    for x in self.use_flags:
      uses[x.replace('-', '_')] = True
    use_args = ['%s=%s' % (x, str(uses[x]).lower()) for x in uses]
    gn_args_args += ['use={%s}' % (' '.join(use_args))]

    gn_args = ['gn', 'gen']
    if self.verbose:
      gn_args += ['-v']
    gn_args += [
        '--root=%s' % self.get_platform2_root(),
        '--args=%s' % ' '.join(gn_args_args),
        self.get_products_path(),
    ]
    cros_build_lib.RunCommand(gn_args, env=buildenv,
                              cwd=self.get_platform2_root())

  def compile(self, args):
    """Runs the compile step of the Platform2 build.

    Removes any existing component markers that may exist (so we don't run
    tests/install for projects that have been disabled since the last
    build). Builds arguments for running Ninja and then runs Ninja.
    """
    for component in self.get_components_glob():
      os.remove(component)

    if self.should_use_gn():
      args = ['%s:%s' % (self.platform_subdir, x) for x in args]
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
  parser.add_argument('--platform_subdir', required=True,
                      help='subdir in platform2 where the package is located')
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
                 options.enable_tests, options.cache_dir, jobs=options.jobs,
                 platform_subdir=options.platform_subdir)
  getattr(p2, options.action)(options.args)


if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
