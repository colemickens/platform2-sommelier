// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides the command "vmc" for crosh which manages containers inside the vm.

use std::process;

use crate::dispatcher::{self, wait_for_result, Arguments, Command, Dispatcher};
use crate::util::is_chrome_feature_enabled;

pub fn register(dispatcher: &mut Dispatcher) {
    dispatcher.register_command(
        Command::new("vmc".to_string(), "".to_string(), "".to_string())
            .set_command_callback(Some(execute_vmc)),
    );
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
