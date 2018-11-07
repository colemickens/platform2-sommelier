// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::collections::BTreeMap;
use std::error::Error;

use backends::Backend;

mod vmc;

use self::vmc::Vmc;

/// A string to string mapping of environment variables to values.
pub type EnvMap<'a> = BTreeMap<&'a str, &'a str>;

/// Each frontend implements a command line style interface that uses a backend to implement
/// commands.
pub trait Frontend {
    /// Get the name of this frontend.
    fn name(&self) -> &str;

    /// Prints the command line style usage of this frontend.
    fn print_usage(&self, program_name: &str);

    /// Parses the command line style `args` and enviroment variables and runs the chosen
    /// command against the given `backend`.
    fn run(&self, backend: &mut Backend, args: &[&str], environ: &EnvMap)
        -> Result<(), Box<Error>>;
}

/// An array of all frontends.
pub const FRONTENDS: &[&Frontend] = &[&Vmc];
