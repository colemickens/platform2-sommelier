// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides the command "vmc" for crosh which manages containers inside the vm.

use std::io::{copy, Write};
use std::process;

use crate::dispatcher::{self, wait_for_result, Arguments, Command, Dispatcher};
use crate::util::is_chrome_feature_enabled;

pub fn register(dispatcher: &mut Dispatcher) {
    dispatcher.register_command(
        Command::new("vmc".to_string(), "".to_string(), "".to_string())
            .set_command_callback(Some(execute_vmc))
            .set_help_callback(vmc_help),
    );
}

fn vmc_help(_cmd: &Command, w: &mut dyn Write, _level: usize) {
    let mut sub = process::Command::new("vmc").arg("--help").spawn().unwrap();

    if copy(&mut sub.stdout.take().unwrap(), w).is_err() {
        panic!();
    }

    if sub.wait().is_err() {
        panic!();
    }
}

fn execute_vmc(_cmd: &Command, args: &Arguments) -> Result<(), dispatcher::Error> {
    if !is_chrome_feature_enabled("IsCrostiniEnabled").unwrap_or(false)
        && !is_chrome_feature_enabled("IsPluginVmEnabled").unwrap_or(false)
    {
        eprintln!("This command is not available.");
        return Err(dispatcher::Error::CommandReturnedError);
    }

    if !is_chrome_feature_enabled("IsVmManagementCliAllowed").unwrap_or(false) {
        eprintln!("This command is not available.");
        return Err(dispatcher::Error::CommandReturnedError);
    }

    wait_for_result(
        process::Command::new("vmc")
            .args(args.get_args())
            .spawn()
            .or(Err(dispatcher::Error::CommandReturnedError))?,
    )
}
