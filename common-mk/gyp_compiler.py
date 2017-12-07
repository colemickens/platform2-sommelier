# -*- coding: utf-8 -*-
# Copyright 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=bad-continuation,docstring-second-line-blank,redefined-builtin

"""Compile logic copied from upstream gyp.input module.

None of this is written by Chromium OS.  We have a copy here so people don't
have to install a copy of gyp itself in their system.

Last synced from:
https://chromium.googlesource.com/external/gyp/
940a15ee3f1c89f193cb4c19373b3f6e9ad15b95
"""

from __future__ import print_function

from compiler.ast import Const
from compiler.ast import Dict
from compiler.ast import Discard
from compiler.ast import List
from compiler.ast import Module
from compiler.ast import Stmt
import compiler


class GypError(Exception):
  """Error class representing an error, which is to be presented
  to the user.  The main entry point will catch and display this.
  """


def CheckedEval(file_contents):
  """Return the eval of a gyp file.

  The gyp file is restricted to dictionaries and lists only, and
  repeated keys are not allowed.

  Note that this is slower than eval() is.
  """

  ast = compiler.parse(file_contents)
  assert isinstance(ast, Module)
  c1 = ast.getChildren()
  assert c1[0] is None
  assert isinstance(c1[1], Stmt)
  c2 = c1[1].getChildren()
  assert isinstance(c2[0], Discard)
  c3 = c2[0].getChildren()
  assert len(c3) == 1
  return CheckNode(c3[0], [])


def CheckNode(node, keypath):
  if isinstance(node, Dict):
    c = node.getChildren()
    dict = {}
    for n in range(0, len(c), 2):
      assert isinstance(c[n], Const)
      key = c[n].getChildren()[0]
      if key in dict:
        raise GypError("Key '" + key + "' repeated at level " +
              repr(len(keypath) + 1) + " with key path '" +
              '.'.join(keypath) + "'")
      kp = list(keypath)  # Make a copy of the list for descending this node.
      kp.append(key)
      dict[key] = CheckNode(c[n + 1], kp)
    return dict
  elif isinstance(node, List):
    c = node.getChildren()
    children = []
    for index, child in enumerate(c):
      kp = list(keypath)  # Copy list.
      kp.append(repr(index))
      children.append(CheckNode(child, kp))
    return children
  elif isinstance(node, Const):
    return node.getChildren()[0]
  else:
    raise TypeError("Unknown AST node at key path '" + '.'.join(keypath) +
         "': " + repr(node))
