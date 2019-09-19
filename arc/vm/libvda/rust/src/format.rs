// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Data structures representing coded/raw formats.

use enumn::N;

use crate::decode_bindings;
use crate::error::*;

/// Represents a video codec.
#[derive(Debug, Clone, Copy, N)]
#[repr(i32)]
pub enum Profile {
    VP8 = decode_bindings::video_codec_profile_VP8PROFILE_MIN,
    VP9Profile0 = decode_bindings::video_codec_profile_VP9PROFILE_PROFILE0,
    H264 = decode_bindings::video_codec_profile_H264PROFILE_MAIN,
}

impl Profile {
    pub(crate) fn to_raw_profile(self) -> decode_bindings::video_codec_profile_t {
        match self {
            Profile::VP8 => decode_bindings::video_codec_profile_VP8PROFILE_MIN,
            Profile::VP9Profile0 => decode_bindings::video_codec_profile_VP9PROFILE_PROFILE0,
            Profile::H264 => decode_bindings::video_codec_profile_H264PROFILE_MAIN,
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
    pub(crate) fn new(f: &decode_bindings::vda_input_format_t) -> Result<InputFormat> {
        let profile = Profile::n(f.profile).ok_or(Error::UnknownProfile(f.profile))?;

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
    YV12 = decode_bindings::video_pixel_format_YV12,
    NV12 = decode_bindings::video_pixel_format_NV12,
}

impl PixelFormat {
    pub(crate) fn new(f: decode_bindings::video_pixel_format_t) -> Result<PixelFormat> {
        PixelFormat::n(f).ok_or(Error::UnknownPixelFormat(f))
    }

    pub(crate) fn to_raw_pixel_format(&self) -> decode_bindings::video_pixel_format_t {
        match *self {
            PixelFormat::YV12 => decode_bindings::video_pixel_format_YV12,
            PixelFormat::NV12 => decode_bindings::video_pixel_format_NV12,
        }
    }
}
