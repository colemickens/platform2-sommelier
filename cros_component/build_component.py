#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to build a component artifact."""

from __future__ import print_function

import json
import os
import re
import zipfile

from distutils.version import LooseVersion

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils

logger = logging.getLogger(__name__)

COMPONENT_ZIP = 'files.zip'
MANIFEST_FILE_NAME = 'manifest.json'
MANIFEST_VERSION_FIELD = u'version'
MANIFEST_PACKAGE_VERSION_FIELD = u'package_version'

def GetParser():
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--component',
                      default='net-print/epson-inkjet-printer-escpr',
                      help='component to build')
  parser.add_argument('--config-path', default='./cros_components.json',
                      help='path to config file.')
  parser.add_argument('--board', default='amd64-generic',
                      help='board to build the component for.')
  parser.add_argument('--platform', default='chromeos_intel64-archive',
                      help='name for the platform folder in Omaha.'
                           '{chromeos_arm32-archive, chromeos_intel64-archive}')
  parser.add_argument('--upload', dest='upload', action='store_true',
                      default=False,
                      help='Upload to Omaha gs bucket.')
  return parser


def ParseVersion(version_str):
  """Parse version string into a list with components.

  Args:
    version_str: (str) version string.

  Returns:
    [int]: a list with version components.
  """
  pattern = re.compile("[0-9]+(\\.[0-9]+){2,3}")
  m = pattern.match(version_str)
  if m:
    return [int(x) for x in m.group().split('.')]
  return []

def CheckGsBucket(gsbucket):
  """Return list of folders in a gs bucket.

  Args:
    gsbucket: (str) gs bucket url.

  Returns:
    [str]: a list of folder paths.
  """
  ctx = gs.GSContext(False)
  dirs = ctx.LS(gsbucket)

  return [x for x in dirs if x != gsbucket]


def GetCurrentVersion(paths):
  """Find the current component version by iterating gsbucket root folder.

  Args:
    paths: ([str]) a list of folder paths strings.

  Returns:
    str: current component version.
    str: gs path for current component version.
  """
  current_version = LooseVersion('0.0.0.0')
  current_version_path = ''

  for version_path in paths:
    if version_path[-1] != '/':
      logger.fatal("version_path (%s) needs to end with '/'.", version_path)
      continue
    version = os.path.basename(version_path[:-1])
    if len(ParseVersion(version)) < 3:
      # path does not contain a component version
      continue
    v = LooseVersion(version)
    if v > current_version:
      current_version = v
      current_version_path = version_path
  return str(current_version), current_version_path


def DecideVersion(version, current_version):
  """Decide the component version

  Each version has release.major.minor[.bugfix] format.
  If release.major.minor are equal, then use current_version as version and
  increase bugfix by 1 (set to be 1 if bugfix is missing). Otherwise, use
  version (package) as final version and set bugfix 1.

  Args:
    version: (str) package version
    current_version: (str) current component version

  Returns:
    str: next component version.
  """
  version = ParseVersion(version)
  current_version = ParseVersion(current_version)
  if (len(version) != 3 and len(version) != 4) or \
     (len(current_version) != 3 and len(current_version)) != 4:
    logger.fatal("version_path is in wrong format")
  if version[0] < current_version[0] or version[1] < current_version[1] or \
     version[2] < current_version[2]:
    logger.fatal("component being built is outdated.")
    return []
  if version[0] == current_version[0] and version[1] == current_version[1] and \
     version[2] == current_version[2]:
    # Rev bug fix on top of current_version.
    version = current_version
    if len(version) < 4:
      version.append(1)
    else:
      version[3] = version[3] + 1
  else:
    # Use package version.1 as next component version.
    if len(version) < 4:
      version.append(1)
    else:
      version[3] = 1
  return '.'.join([str(x) for x in version])


def CheckValidMetadata(metadata):
  """Check if metadata in configuration is valid.

  Args:
    metadata: (str) metadata in component configs.

  Returns:
    bool: if metadata is valid.
  """
  if not "files" in metadata or \
      not "gsbucket" in metadata or \
      not "pkgpath" in metadata or \
      not "name" in metadata or \
      not "manifest" in metadata:
    logger.fatal("attribute is missing.")
    return False
  else:
    return True


def CheckComponentFilesExistence(paths):
  """Check if paths exist.

  Args:
    paths: ([str]) a list of path.

  Returns:
    bool: true if all paths exists.
  """
  for path in paths:
    if not os.path.exists(path):
      logger.fatal('component file is missing:%s', path)
      return False
  return True


def UploadComponent(component_dir, gsbucket):
  """Upload a component.

  Args:
    component_dir: (str) location for generated component.
    gsbucket: (str) gs bucket to upload.
  """
  logger.info('upload %s to %s', component_dir, gsbucket)
  ctx = gs.GSContext(False)
  # Upload component to gs.
  ctx.DoCommand(['cp', '-r', component_dir, gsbucket])


def CreateComponent(manifest_path, version, package_version, platform, files,
                    upload, gsbucket):
  """Create component zip file.

  Args:
    manifest_path: (str) path to raw manifest file.
    version: (str) component version.
    package_version: (str) package version.
    platform: (str) platform folder name on Omaha.
    files: ([str]) paths for component files.
    upload: (bool) whether to upload the generate component to Omaha.
    gsbucket: (str) Omaha gsbucket path.
  """
  if not os.path.exists(manifest_path):
    logger.fatal('manifest file is missing:%s', manifest_path)
  with open(manifest_path) as f:
    # Construct final manifest file.
    data = json.load(f)
    data[MANIFEST_VERSION_FIELD] = version
    data[MANIFEST_PACKAGE_VERSION_FIELD] = package_version
    # Create final zip file of the component and store it to a temp folder.
    with osutils.TempDir(prefix='component_') as tempdir:
      component_folder = os.path.join(tempdir, data[MANIFEST_VERSION_FIELD],
                                      platform)
      os.makedirs(component_folder)
      component_zipfile = os.path.join(component_folder, COMPONENT_ZIP)
      zf = zipfile.ZipFile(component_zipfile, 'w', zipfile.ZIP_DEFLATED)
      # Move component files into zip file.
      for f in files:
        zf.write(f, os.path.basename(f))
      # Write manifest file into zip file.
      zf.writestr(MANIFEST_FILE_NAME, json.dumps(data))
      logger.info('component is generated at %s', zf.filename)
      zf.close()

      # Upload component to gs bucket.
      if upload:
        UploadComponent(os.path.join(tempdir, data[MANIFEST_VERSION_FIELD]),
                        gsbucket)

def GetCurrentPackageVersion(current_version_path, platform):
  """Get package version of current component.

  Args:
    current_version_path: (str) path to current version component.
    platform: (str) platform name in omaha.

  Returns:
    str: package version of current component.
  """
  ctx = gs.GSContext(False)
  src = os.path.join(current_version_path, platform, COMPONENT_ZIP)
  if len(current_version_path) != 0 and ctx.Exists(src):
    with osutils.TempDir(prefix='component_') as tempdir:
      ctx.Copy(src, tempdir)
      cros_build_lib.RunCommand(
          ['unzip', '-o', '-d', tempdir, os.path.join(tempdir, COMPONENT_ZIP)],
          redirect_stdout=True, redirect_stderr=True)
      with open(os.path.join(tempdir, MANIFEST_FILE_NAME)) as f:
        manifest = json.load(f)
        if MANIFEST_PACKAGE_VERSION_FIELD in manifest:
          return manifest[MANIFEST_PACKAGE_VERSION_FIELD]
  return '0.0.0.0'

def IsPackage(folder_name, package_name):
  """Check if folder is for a package.

  Args:
    folder_name: (str) name of the folder.
    package_name: (str) name of the package.

  Returns:
    Bool: whether folder is for the package.
  """
  pattern = re.compile('(^[\\w-]*)-[0-9]+(\\.[0-9]+){2}(-r[0-9]+){0,1}$')
  m = pattern.match(folder_name)
  return m.group(1) == package_name

def BuildComponent(component_to_build, components, board, platform,
                   upload=False):
  """Build a component.

  Args:
    component_to_build: (str) component to build.
    components: ([object]) a list of components.
    board: (str) board to build the component on.
    platform: (str) platform name in omaha.
    upload: (bool) True if uploading to Omaha; False if not uploading to Omaha.
  """
  for component in components:
    for pkg, metadata in component.iteritems():
      if pkg == component_to_build:
        if not CheckValidMetadata(metadata):
          continue
        logger.info('build component:%s', pkg)
        # Check if component files are built successfully.
        files = [os.path.join(cros_build_lib.GetSysroot(), 'build', board, x) \
                 for x in metadata["files"]]
        if not CheckComponentFilesExistence(files):
          logger.fatal('component files are missing.')
          return

        # Check release versions on gs.
        gsbucket = metadata['gsbucket']
        dirs = CheckGsBucket(gsbucket)
        if len(dirs) == 0:
          logger.fatal('gsbucket %s has no subfolders', gsbucket)
        logger.info('Dirs in gsbucket:%s', dirs)
        current_version, current_version_path = GetCurrentVersion(dirs)
        logger.info('latest component version on Omaha gs: %s', current_version)
        # Get package version of current component.
        current_package_version = GetCurrentPackageVersion(current_version_path,
                                                           platform)

        # Check component (gentoo package) version.
        name = metadata["name"]
        for f in os.listdir(os.path.join(cros_build_lib.GetSysroot(), 'build',
                                         board, metadata["pkgpath"])):
          if IsPackage(f, name):
            # Decide component's next release version.
            package_version = f[len(name)+1:]
            logger.info('current package version: %s', package_version)
            logger.info('package version of current component: %s',
                        current_package_version)
            if package_version == current_package_version:
              logger.info('component on Omaha is already up to date.')
              return
            version = DecideVersion(package_version, current_version)
            logger.info('next component version on Omaha gs: %s', version)

            manifest_path = os.path.join(cros_build_lib.GetSysroot(), 'build',
                                         board, metadata["manifest"])

            CreateComponent(manifest_path, version, package_version, platform,
                            files, upload, gsbucket)
            return


def GetComponentsToBuild(path):
  """Parse components from config file.

  Args:
    path: (str) file path to config file.

  Returns:
    Object: a json object of config file content.
  """
  with open(path) as f:
    return json.load(f)


def main(argv):
  opts = GetParser().parse_args(argv)
  BuildComponent(opts.component,
                 GetComponentsToBuild(opts.config_path),
                 opts.board, opts.platform,
                 opts.upload)

if __name__ == '__main__':
  commandline.ScriptWrapperMain(lambda _: main)
