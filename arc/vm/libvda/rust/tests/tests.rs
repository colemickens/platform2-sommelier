// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Integration tests using LibVDA fake implemenation.

use libvda::*;

fn create_vda_instance() -> VdaInstance {
    VdaInstance::new(VdaImplType::Fake).expect("failed to create VDAInstance")
}

#[test]
fn test_create_instance() {
    let instance = create_vda_instance();
    let caps = instance.get_capabilities();

    assert_ne!(caps.input_formats.len(), 0);
    assert_ne!(caps.output_formats.len(), 0);
}

#[test]
fn test_initialize_decode_session() {
    let instance = create_vda_instance();
    let _session = instance
        .open_session(Profile::VP8)
        .expect("failed to open a session for VP8");
    // `drop(instance)` here must cause a compile error because `_session` must be dropped in
    // advance.
}
