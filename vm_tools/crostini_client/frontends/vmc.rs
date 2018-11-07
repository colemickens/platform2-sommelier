// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::error::Error;
use std::fmt;

use backends::Backend;
use frontends::Frontend;
use EnvMap;

enum VmcError {
    Command(&'static str, u32, Box<Error>),
    ExpectedCrosUserIdHash,
    ExpectedName,
    ExpectedVmAndContainer,
    ExpectedVmAndFileName,
    ExpectedVmAndPath,
    ExpectedNoArgs,
    InvalidEmail,
    MissingActiveSession,
    UnknownSubcommand(String),
}

use self::VmcError::*;

// Remove useless expression items that the `try_command!()` macro captures and stringifies when
// generating a `VmcError::Command`.
fn trim_routine(s: &str) -> String {
    s.trim_start_matches("self.backend.")
        .replace(char::is_whitespace, "")
}

impl fmt::Display for VmcError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Command(routine, line_num, e) => write!(
                f,
                "routine at {}:{} `{}` failed: {}",
                file!(),
                line_num,
                trim_routine(&routine),
                e
            ),
            ExpectedCrosUserIdHash => write!(f, "expected CROS_USER_ID_HASH environment variable"),
            ExpectedName => write!(f, "expected <name>"),
            ExpectedVmAndContainer => write!(
                f,
                "expected <vm name> <container name> [ <image server> <image alias> ]"
            ),
            ExpectedVmAndFileName => {
                write!(f, "expected <vm name> <file name> [removable storage name]")
            }
            ExpectedVmAndPath => write!(f, "expected <vm name> <path>"),
            ExpectedNoArgs => write!(f, "expected no arguments"),
            InvalidEmail => write!(f, "the active session has an invalid email address"),
            MissingActiveSession => write!(
                f,
                "missing active session corresponding to $CROS_USER_ID_HASH"
            ),
            UnknownSubcommand(s) => write!(f, "no such subcommand: `{}`", s),
        }
    }
}

impl fmt::Debug for VmcError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <Self as fmt::Display>::fmt(self, f)
    }
}

impl Error for VmcError {}

type VmcResult = Result<(), Box<Error>>;

macro_rules! try_command {
    ($x:expr) => {
        $x.map_err(|e| Command(stringify!($x), line!(), e))?
    };
}

struct Command<'a, 'b, 'c> {
    backend: &'a mut Backend,
    args: &'b [&'b str],
    environ: &'c EnvMap<'c>,
}

impl<'a, 'b, 'c> Command<'a, 'b, 'c> {
    fn start(&mut self) -> VmcResult {
        if self.args.len() != 1 {
            return Err(ExpectedName.into());
        }

        let vm_name = self.args[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        try_command!(self.backend.metrics_send_sample("Vm.VmcStart"));
        try_command!(self.backend.vm_start(vm_name, user_id_hash));
        try_command!(self.backend.metrics_send_sample("Vm.VmcStartSuccess"));
        try_command!(self.backend.vsh_exec(vm_name, user_id_hash));

        Ok(())
    }

    fn stop(&mut self) -> VmcResult {
        if self.args.len() != 1 {
            return Err(ExpectedName.into());
        }

        let vm_name = self.args[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        try_command!(self.backend.vm_stop(vm_name, user_id_hash));

        Ok(())
    }

    fn destroy(&mut self) -> VmcResult {
        if self.args.len() != 1 {
            return Err(ExpectedName.into());
        }

        let vm_name = self.args[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        match self.backend.disk_destroy(vm_name, user_id_hash) {
            Ok(()) => Ok(()),
            Err(e) => {
                try_command!(self.backend.metrics_send_sample("Vm.DiskEraseFailed"));
                Err(Command("disk_destroy", line!(), e).into())
            }
        }
    }

    fn export(&mut self) -> VmcResult {
        let (vm_name, file_name, removable_media) = match self.args.len() {
            2 => (self.args[0], self.args[1], None),
            3 => (self.args[0], self.args[1], Some(self.args[2])),
            _ => return Err(ExpectedVmAndFileName.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        try_command!(
            self.backend
                .vm_export(vm_name, user_id_hash, file_name, removable_media)
        );
        Ok(())
    }

    fn list(&mut self) -> VmcResult {
        if !self.args.is_empty() {
            return Err(ExpectedNoArgs.into());
        }

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let (disk_image_list, total_size) = try_command!(self.backend.disk_list(user_id_hash));
        for disk_image in disk_image_list {
            println!("{}", disk_image);
        }
        println!("Total Size (bytes): {}", total_size);
        Ok(())
    }

    fn share(&mut self) -> VmcResult {
        if self.args.len() != 2 {
            return Err(ExpectedVmAndPath.into());
        }

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let vm_name = self.args[0];
        let path = self.args[1];
        let vm_path = try_command!(self.backend.vm_share_path(vm_name, user_id_hash, path));
        println!("{} is available at path {}", path, vm_path);
        Ok(())
    }

    fn container(&mut self) -> VmcResult {
        let (vm_name, container_name, image_server, image_alias) = match self.args.len() {
            2 => (
                self.args[0],
                self.args[1],
                "https://storage.googleapis.com/cros-containers",
                "debian/stretch",
            ),
            4 => (self.args[0], self.args[1], self.args[2], self.args[3]),
            _ => return Err(ExpectedVmAndContainer.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let sessions = try_command!(self.backend.sessions_list());
        let email = sessions
            .iter()
            .find(|(_, hash)| hash == user_id_hash)
            .map(|(email, _)| email)
            .ok_or(MissingActiveSession)?;

        let username = match email.find('@') {
            Some(0) | None => return Err(InvalidEmail.into()),
            Some(end) => &email[..end],
        };

        try_command!(self.backend.container_create(
            vm_name,
            user_id_hash,
            container_name,
            image_server,
            image_alias
        ));
        try_command!(
            self.backend
                .container_start(vm_name, user_id_hash, container_name)
        );
        try_command!(self.backend.container_setup_user(
            vm_name,
            user_id_hash,
            container_name,
            username
        ));
        try_command!(
            self.backend
                .vsh_exec_container(vm_name, user_id_hash, container_name)
        );

        Ok(())
    }
}

const USAGE: &str = r#"
   [ start <name> |
     stop <name> |
     destroy <name> |
     export <vm name> <file name> [removable storage name] |
     list |
     share <vm name> <path> |
     container <vm name> <container name> [ <image server> <image alias> ] ]
"#;

/// A frontend that implements a `vmc` (Virtual Machine Controller) style command line interface.
/// This is the interface accessible from crosh (Ctrl-Alt-T in the browser to access).
pub struct Vmc;

impl Frontend for Vmc {
    fn name(&self) -> &str {
        "vmc"
    }

    fn print_usage(&self, program_name: &str) {
        eprintln!("USAGE: {}{}", program_name, USAGE);
    }

    fn run(&self, backend: &mut Backend, args: &[&str], environ: &EnvMap) -> VmcResult {
        if args.len() < 2 {
            self.print_usage("vmc");
            return Ok(());
        }

        let mut command = Command {
            backend,
            args: &args[2..],
            environ,
        };

        let command_name = args[1];
        match command_name {
            "start" => command.start(),
            "stop" => command.stop(),
            "destroy" => command.destroy(),
            "export" => command.export(),
            "list" => command.list(),
            "share" => command.share(),
            "container" => command.container(),
            _ => Err(UnknownSubcommand(command_name.to_owned()).into()),
        }
    }
}
