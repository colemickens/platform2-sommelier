// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

This directory contains fides, a Chromium service for general purpose device
configuration management.

fidesd is a system daemon that maintains device configuration represented in the
form of key-value pairs. It is responsible for persisting currently effective
device configuration and exposing it to consumers, including change
notifications.

All device configuration is subject to trust checks. To this end, fides
validates all device configuration to be originating from trusted configuration
sources, both when receiving configuration updates and when loading device
configuration from disk.

In order to facilitate trust checks, fides includes a trust delegation framework
that allows trust to be extended from a trust anchor such as the device owner or
the root file system (subject to verified boot protection) to other devices,
users, remote management services etc.
