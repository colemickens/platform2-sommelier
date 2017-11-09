# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Distributed under the terms of the GNU General Public License v2

# FIXME(lannm): Temporary workaround for ebuilds while reorganizing modules.

import runpy

runpy.run_module('cros_config_host.validate_config')
