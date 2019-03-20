// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern crate pkg_config;

fn main() {
    pkg_config::probe_library("libvda").unwrap();
}
