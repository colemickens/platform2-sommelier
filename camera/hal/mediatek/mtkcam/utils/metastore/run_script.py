#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""helper to run the given shell script"""

from __future__ import print_function
import subprocess
import sys

subprocess.call([sys.argv[1], sys.argv[2], sys.argv[3]])
