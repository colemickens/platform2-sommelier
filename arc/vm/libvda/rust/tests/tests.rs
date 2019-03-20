// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Integration tests using LibVDA fake implemenation.

use libvda::*;

#[test]
fn test_create_instance() {
    let instance = VdaInstance::new(VdaImplType::Fake)
        .map_err(|msg| panic!("failed to create VdaInstance: {}", msg))
        .unwrap();
    let caps = instance.get_capabilities();

    assert_ne!(caps.input_formats.len(), 0);
    assert_ne!(caps.output_formats.len(), 0);
}
