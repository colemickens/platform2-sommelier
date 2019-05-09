// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::marker::PhantomData;
use std::os::raw::c_void;

use crate::bindings;
use crate::format::Profile;
use crate::vda_instance::VdaInstance;

/// Represents a decode session.
pub struct Session<'a> {
    vda_ptr: *mut c_void,
    raw_ptr: *mut bindings::vda_session_info_t,
    // `phantom` gurantees that `Session` won't outlive the lifetime of `VdaInstance` that owns
    // `vda_ptr`.
    phantom: PhantomData<&'a VdaInstance>,
}

impl<'a> Session<'a> {
    /// Creates a new `Session`.
    /// This function is safe if `vda_ptr` is a non-NULL pointer obtained from
    /// `bindings::initialize`.
    pub(crate) unsafe fn new(vda_ptr: *mut c_void, profile: Profile) -> Option<Self> {
        // `init_decode_session` is safe if `vda_ptr` is a non-NULL pointer from
        // `bindings::initialize`.
        let raw_ptr: *mut bindings::vda_session_info_t =
            bindings::init_decode_session(vda_ptr, profile.to_raw_profile());

        if raw_ptr.is_null() {
            return None;
        }

        Some(Session {
            vda_ptr,
            raw_ptr,
            phantom: PhantomData,
        })
    }
}

impl<'a> Drop for Session<'a> {
    fn drop(&mut self) {
        // Safe because `vda_ptr` and `raw_ptr` are unchanged from the time `new` was called.
        // Also, `vda_ptr` is valid because `phantom` guranteed that `VdaInstance` owning `vda_ptr`
        // is not dropped yet.
        unsafe {
            bindings::close_decode_session(self.vda_ptr, self.raw_ptr);
        }
    }
}
