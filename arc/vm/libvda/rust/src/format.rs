// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Data structures representing coded/raw formats.

use enumn::N;

use crate::bindings;
use crate::error::*;

/// Represents a video codec.
#[derive(Debug, Clone, Copy, N)]
#[repr(i32)]
pub enum Profile {
    VP8 = bindings::vda_profile_VP8PROFILE_MIN,
    VP9Profile0 = bindings::vda_profile_VP9PROFILE_PROFILE0,
    H264 = bindings::vda_profile_H264PROFILE_MAIN,
}

impl Profile {
    pub(crate) fn to_raw_profile(self) -> bindings::vda_profile_t {
        match self {
            Profile::VP8 => bindings::vda_profile_VP8PROFILE_MIN,
            Profile::VP9Profile0 => bindings::vda_profile_VP9PROFILE_PROFILE0,
            Profile::H264 => bindings::vda_profile_H264PROFILE_MAIN,
        }
    }
}

/// Represents an input video format for VDA.
pub struct InputFormat {
    pub profile: Profile,
    pub min_width: u32,
    pub min_height: u32,
    pub max_width: u32,
    pub max_height: u32,
}

impl InputFormat {
    pub(crate) fn new(f: &bindings::vda_input_format_t) -> VdaResult<InputFormat> {
        let profile = Profile::n(f.profile).ok_or_else(|| VdaError::UnknownProfile(f.profile))?;

        Ok(InputFormat {
            profile,
            min_width: f.min_width,
            min_height: f.min_height,
            max_width: f.max_width,
            max_height: f.max_height,
        })
    }
}

/// Represents a raw pixel format.
#[derive(Debug, N)]
#[repr(u32)]
pub enum PixelFormat {
    YV12 = bindings::vda_pixel_format_YV12,
    NV12 = bindings::vda_pixel_format_NV12,
}

impl PixelFormat {
    pub(crate) fn new(f: bindings::vda_pixel_format_t) -> VdaResult<PixelFormat> {
        PixelFormat::n(f).ok_or_else(|| VdaError::UnknownPixelFormat(f))
    }
}
