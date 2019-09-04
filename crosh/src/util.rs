// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides helper functions used by handler implementations of crosh commands.

use std::error;
use std::fmt::{self, Display};
use std::fs::read_to_string;
use std::io::Read;
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};

use regex::Regex;

static INCLUDE_DEV: AtomicBool = AtomicBool::new(false);
static INCLUDE_USB: AtomicBool = AtomicBool::new(false);

pub enum Error {
    NoMatchFound,
    WrappedError(String),
}

impl Display for Error {
    #[remain::check]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Error::*;

        #[sorted]
        match self {
            NoMatchFound => write!(f, "No match found."),
            WrappedError(err) => write!(f, "{}", err),
        }
    }
}

impl<T: error::Error> From<T> for Error {
    fn from(err: T) -> Self {
        Error::WrappedError(format!("{:?}", err).to_string())
    }
}

pub fn is_dev_mode() -> Result<bool, Error> {
    let output = Command::new("crossystem").arg("cros_debug?1").output()?;
    Ok(output.status.success())
}

pub fn is_removable() -> Result<bool, Error> {
    let dev = root_dev()?;
    let groups = Regex::new(r#"/dev/([^/]+?)p?[0-9]+$"#)?
        .captures(&dev)
        .ok_or(Error::NoMatchFound)?;

    let dev = groups.get(1).unwrap().as_str();

    match read_to_string(format!("/sys/block/{}/removable", dev)) {
        Ok(contents) => Ok(contents.trim() == "1"),
        Err(err) => Err(err.into()),
    }
}

pub fn set_dev_commands_included(value: bool) {
    INCLUDE_DEV.store(value, Ordering::Release);
}

pub fn set_usb_commands_included(value: bool) {
    INCLUDE_USB.store(value, Ordering::Release);
}

pub fn dev_commands_included() -> bool {
    INCLUDE_DEV.load(Ordering::Acquire)
}

pub fn usb_commands_included() -> bool {
    INCLUDE_USB.load(Ordering::Acquire)
}

fn root_dev() -> Result<String, Error> {
    let mut child = Command::new("rootdev")
        .arg("-s")
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .spawn()?;

    let mut result = String::new();
    child.stdout.take().unwrap().read_to_string(&mut result)?;
    child.wait()?;

    Ok(result.trim().to_string())
}
