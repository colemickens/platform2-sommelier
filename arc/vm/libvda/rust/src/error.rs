// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Errors that can happen in LibVDA.

use std::error;
use std::fmt::{self, Display};

pub type VdaResult<T> = std::result::Result<T, VdaError>;

#[derive(Debug)]
pub enum VdaError {
    GetCapabilitiesFailure,
    InstanceInitFailure,
    InvalidCapabilities(String),
    UnknownPixelFormat(u32),
    UnknownProfile(i32),
}

impl Display for VdaError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::VdaError::*;

        match self {
            GetCapabilitiesFailure => write!(f, "failed to get capabilities"),
            InstanceInitFailure => write!(f, "failed to initialize VDA instance"),
            InvalidCapabilities(e) => write!(f, "obtained capabilities are invalid: {}", e),
            UnknownPixelFormat(p) => write!(f, "unknown pixel format: {}", p),
            UnknownProfile(p) => write!(f, "unknown profile: {}", p),
        }
    }
}

impl error::Error for VdaError {}
