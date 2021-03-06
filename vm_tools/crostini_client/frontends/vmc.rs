// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::error::Error;
use std::fmt;
use std::io::{stdout, Write};

use getopts::Options;

use backends::{Backend, ContainerSource, VmFeatures};
use frontends::Frontend;
use EnvMap;

enum VmcError {
    Command(&'static str, u32, Box<dyn Error>),
    ExpectedCrosUserIdHash,
    ExpectedName,
    ExpectedNoArgs,
    ExpectedU8Bus,
    ExpectedU8Device,
    ExpectedU8Port,
    ExpectedUUID,
    ExpectedVmAndContainer,
    ExpectedVmAndFileName,
    ExpectedVmAndMaybeFileName,
    ExpectedVmAndPath,
    ExpectedVmBusDevice,
    ExpectedVmPort,
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
            ExpectedVmAndMaybeFileName => write!(
                f,
                "expected <vm name> [<file name> [removable storage name]]"
            ),
            ExpectedVmAndPath => write!(f, "expected <vm name> <path>"),
            ExpectedVmBusDevice => write!(f, "expected <vm name> <bus>:<device>"),
            ExpectedNoArgs => write!(f, "expected no arguments"),
            ExpectedU8Bus => write!(f, "expected <bus> to fit into an 8-bit integer"),
            ExpectedU8Device => write!(f, "expected <device> to fit into an 8-bit integer"),
            ExpectedU8Port => write!(f, "expected <port> to fit into an 8-bit integer"),
            ExpectedUUID => write!(f, "expected <command UUID>"),
            ExpectedVmPort => write!(f, "expected <vm name> <port>"),
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

type VmcResult = Result<(), Box<dyn Error>>;

macro_rules! try_command {
    ($x:expr) => {
        $x.map_err(|e| Command(stringify!($x), line!(), e))?
    };
}

struct Command<'a, 'b, 'c> {
    backend: &'a mut dyn Backend,
    args: &'b [&'b str],
    environ: &'c EnvMap<'c>,
}

impl<'a, 'b, 'c> Command<'a, 'b, 'c> {
    // Metrics are on a best-effort basis. We print errors related to sending metrics, but stop
    // propagation of the error, which is why this function never returns an error.
    fn metrics_send_sample(&mut self, name: &str) {
        if let Err(e) = self.backend.metrics_send_sample(name) {
            eprintln!(
                "warning: failed attempt to send metrics sample `{}`: {}",
                name, e
            );
        }
    }

    fn start(&mut self) -> VmcResult {
        let mut opts = Options::new();
        opts.optflag("", "enable-gpu", "when starting the vm, enable gpu support");
        opts.optflag(
            "",
            "software-tpm",
            "provide software-based virtual Trusted Platform Module",
        );
        opts.optflag(
            "",
            "enable-audio-capture",
            "when starting the vm, enable audio capture support",
        );
        let matches = opts.parse(self.args)?;

        if matches.free.len() != 1 {
            return Err(ExpectedName.into());
        }

        let vm_name = &matches.free[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;
        let features = VmFeatures {
            gpu: matches.opt_present("enable-gpu"),
            software_tpm: matches.opt_present("software-tpm"),
            audio_capture: matches.opt_present("enable-audio-capture"),
        };

        self.metrics_send_sample("Vm.VmcStart");
        try_command!(self.backend.vm_start(vm_name, user_id_hash, features));
        self.metrics_send_sample("Vm.VmcStartSuccess");
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

    fn create(&mut self) -> VmcResult {
        let plugin_vm = self.args.len() > 0 && self.args[0] == "-p";
        if plugin_vm {
            // Discard the first argument (-p).
            self.args = &self.args[1..];
        }

        let mut s = self.args.splitn(2, |arg| *arg == "--");
        let args = s.next().expect("failed to split argument list");
        let params = s.next().unwrap_or(&[]);

        let (vm_name, file_name, removable_media) = match args.len() {
            1 => (args[0], None, None),
            2 => (args[0], Some(args[1]), None),
            3 => (args[0], Some(args[1]), Some(args[2])),
            _ => return Err(ExpectedVmAndMaybeFileName.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        if let Some(uuid) = try_command!(self.backend.vm_create(
            vm_name,
            user_id_hash,
            plugin_vm,
            file_name,
            removable_media,
            params,
        )) {
            println!("VM creation in progress: {}", uuid);
            self.wait_disk_op_completion(&uuid, user_id_hash)?;
        }
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
                self.metrics_send_sample("Vm.DiskEraseFailed");
                Err(Command("disk_destroy", line!(), e).into())
            }
        }
    }

    fn wait_disk_op_completion(&mut self, uuid: &str, user_id_hash: &str) -> VmcResult {
        loop {
            let (done, progress) = try_command!(self.backend.wait_disk_op(&uuid, user_id_hash));
            if done {
                println!("\rOperation completed successfully");
                return Ok(());
            }

            print!("\rOperation in progress: {}% done", progress);
            stdout().flush()?;
        }
    }

    fn export(&mut self) -> VmcResult {
        let generate_digest = self.args.len() > 0 && self.args[0] == "-d";
        if generate_digest {
            // Discard the first argument (-d).
            self.args = &self.args[1..];
        }

        let (vm_name, file_name, removable_media) = match self.args.len() {
            2 => (self.args[0], self.args[1], None),
            3 => (self.args[0], self.args[1], Some(self.args[2])),
            _ => return Err(ExpectedVmAndFileName.into()),
        };

        let digest_name = file_name.to_owned() + ".sha256.txt";
        let digest_option = if generate_digest {
            Some(digest_name.as_str())
        } else {
            None
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        if let Some(uuid) = try_command!(self.backend.vm_export(
            vm_name,
            user_id_hash,
            file_name,
            digest_option,
            removable_media
        )) {
            println!("Export in progress: {}", uuid);
            self.wait_disk_op_completion(&uuid, user_id_hash)?;
        }
        Ok(())
    }

    fn import(&mut self) -> VmcResult {
        let plugin_vm = self.args.len() > 0 && self.args[0] == "-p";
        if plugin_vm {
            // Discard the first argument (-p).
            self.args = &self.args[1..];
        }
        let (vm_name, file_name, removable_media) = match self.args.len() {
            2 => (self.args[0], self.args[1], None),
            3 => (self.args[0], self.args[1], Some(self.args[2])),
            _ => return Err(ExpectedVmAndFileName.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        if let Some(uuid) = try_command!(self.backend.vm_import(
            vm_name,
            user_id_hash,
            plugin_vm,
            file_name,
            removable_media
        )) {
            println!("Import in progress: {}", uuid);
            self.wait_disk_op_completion(&uuid, user_id_hash)?;
        }
        Ok(())
    }

    fn disk_op_status(&mut self) -> VmcResult {
        if self.args.len() != 1 {
            return Err(ExpectedUUID.into());
        }

        let uuid = self.args[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let (done, progress) = try_command!(self.backend.disk_op_status(uuid, user_id_hash));
        if done {
            println!("Operation completed successfully");
        } else {
            println!("Operation in progress: {}% done", progress);
        }
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
        for (name, size) in disk_image_list {
            println!("{} ({} bytes)", name, size);
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

    fn unshare(&mut self) -> VmcResult {
        if self.args.len() != 2 {
            return Err(ExpectedVmAndPath.into());
        }

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let vm_name = self.args[0];
        let path = self.args[1];
        try_command!(self.backend.vm_unshare_path(vm_name, user_id_hash, path));
        Ok(())
    }

    fn container(&mut self) -> VmcResult {
        let (vm_name, container_name, source) = match self.args.len() {
            2 => (
                self.args[0],
                self.args[1],
                ContainerSource::ImageServer {
                    image_server: "https://storage.googleapis.com/cros-containers/%d".to_string(),
                    image_alias: "debian/buster".to_string(),
                },
            ),
            4 => (
                self.args[0],
                self.args[1],
                // If this argument looks like an absolute path, treat it and the following
                // parameter as local paths to tarballs.  Otherwise, assume they are an
                // image server URL and image alias.
                if self.args[2].starts_with("/") {
                    ContainerSource::Tarballs {
                        rootfs_path: self.args[2].to_string(),
                        metadata_path: self.args[3].to_string(),
                    }
                } else {
                    ContainerSource::ImageServer {
                        image_server: self.args[2].to_string(),
                        image_alias: self.args[3].to_string(),
                    }
                },
            ),
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

        try_command!(self
            .backend
            .container_create(vm_name, user_id_hash, container_name, source));
        try_command!(self.backend.container_setup_user(
            vm_name,
            user_id_hash,
            container_name,
            username
        ));
        try_command!(self
            .backend
            .container_start(vm_name, user_id_hash, container_name));
        try_command!(self
            .backend
            .vsh_exec_container(vm_name, user_id_hash, container_name));

        Ok(())
    }

    fn usb_attach(&mut self) -> VmcResult {
        let (vm_name, bus_device) = match self.args.len() {
            2 => (self.args[0], self.args[1]),
            _ => return Err(ExpectedVmBusDevice.into()),
        };

        let mut bus_device_parts = bus_device.splitn(2, ':');
        let (bus, device) = match (bus_device_parts.next(), bus_device_parts.next()) {
            (Some(bus_str), Some(device_str)) => (
                bus_str.parse().or(Err(ExpectedU8Bus))?,
                device_str.parse().or(Err(ExpectedU8Device))?,
            ),
            _ => return Err(ExpectedVmBusDevice.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let guest_port = try_command!(self.backend.usb_attach(vm_name, user_id_hash, bus, device));

        println!(
            "usb device at bus={} device={} attached to vm {} at port={}",
            bus, device, vm_name, guest_port
        );

        Ok(())
    }

    fn usb_detach(&mut self) -> VmcResult {
        let (vm_name, port) = match self.args.len() {
            2 => (self.args[0], self.args[1].parse().or(Err(ExpectedU8Port))?),
            _ => return Err(ExpectedVmPort.into()),
        };

        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        try_command!(self.backend.usb_detach(vm_name, user_id_hash, port));

        println!("usb device detached from port {}", port);

        Ok(())
    }

    fn usb_list(&mut self) -> VmcResult {
        if self.args.len() != 1 {
            return Err(ExpectedName.into());
        }

        let vm_name = self.args[0];
        let user_id_hash = self
            .environ
            .get("CROS_USER_ID_HASH")
            .ok_or(ExpectedCrosUserIdHash)?;

        let devices = try_command!(self.backend.usb_list(vm_name, user_id_hash));
        if devices.is_empty() {
            println!("No attached usb devices");
        }
        for (port, vendor_id, product_id, name) in devices {
            println!(
                "Port {:03} ID {:04x}:{:04x} {}",
                port, vendor_id, product_id, name
            );
        }

        Ok(())
    }
}

const USAGE: &str = r#"
   [ start [--enable-gpu] [--enable-audio-capture] <name> |
     stop <name> |
     create [-p] <name> [<source media> [<removable storage name>]] [-- additional parameters]
     destroy <name> |
     disk-op-status <command UUID> |
     export [-d] <vm name> <file name> [<removable storage name>] |
     import [-p] <vm name> <file name> [<removable storage name>] |
     list |
     share <vm name> <path> |
     unshare <vm name> <path> |
     container <vm name> <container name> [ (<image server> <image alias>) | (<rootfs path> <metadata path>)] |
     usb-attach <vm name> <bus>:<device> |
     usb-detach <vm name> <port> |
     usb-list <vm name> |
     --help | -h ]
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

    fn run(&self, backend: &mut dyn Backend, args: &[&str], environ: &EnvMap) -> VmcResult {
        if args.len() < 2 {
            self.print_usage("vmc");
            return Ok(());
        }

        for &arg in args {
            match arg {
                "--" => break,
                "--help" | "-h" => {
                    self.print_usage("vmc");
                    return Ok(());
                }
                _ => {}
            }
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
            "create" => command.create(),
            "destroy" => command.destroy(),
            "export" => command.export(),
            "import" => command.import(),
            "disk-op-status" => command.disk_op_status(),
            "list" => command.list(),
            "share" => command.share(),
            "unshare" => command.unshare(),
            "container" => command.container(),
            "usb-attach" => command.usb_attach(),
            "usb-detach" => command.usb_detach(),
            "usb-list" => command.usb_list(),
            _ => Err(UnknownSubcommand(command_name.to_owned()).into()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use backends::DummyDefaultBackend;

    #[test]
    fn dummy_default_backend() {
        const DUMMY_SUCCESS_ARGS: &[&[&str]] = &[
            &["vmc", "start", "termina"],
            &["vmc", "start", "--enable-gpu", "termina"],
            &["vmc", "start", "termina", "--enable-gpu"],
            &["vmc", "start", "termina", "--software-tpm"],
            &["vmc", "start", "termina", "--enable-gpu", "--software-tpm"],
            &["vmc", "start", "termina", "--enable-audio-capture"],
            &[
                "vmc",
                "start",
                "termina",
                "--enable-audio-capture",
                "--enable-gpu",
            ],
            &[
                "vmc",
                "start",
                "termina",
                "--enable-gpu",
                "--enable-audio-capture",
            ],
            &["vmc", "stop", "termina"],
            &["vmc", "create", "termina"],
            &["vmc", "create", "-p", "termina"],
            &[
                "vmc",
                "create",
                "-p",
                "termina",
                "file name",
                "removable media",
            ],
            &["vmc", "create", "-p", "termina", "--"],
            &["vmc", "create", "-p", "termina", "--", "param"],
            &["vmc", "destroy", "termina"],
            &["vmc", "disk-op-status", "12345"],
            &["vmc", "export", "termina", "file name"],
            &["vmc", "export", "-d", "termina", "file name"],
            &["vmc", "export", "termina", "file name", "removable media"],
            &[
                "vmc",
                "export",
                "-d",
                "termina",
                "file name",
                "removable media",
            ],
            &["vmc", "import", "termina", "file name"],
            &["vmc", "import", "termina", "file name", "removable media"],
            &["vmc", "import", "-p", "termina", "file name"],
            &[
                "vmc",
                "import",
                "-p",
                "termina",
                "file name",
                "removable media",
            ],
            &["vmc", "list"],
            &["vmc", "share", "termina", "my-folder"],
            &["vmc", "unshare", "termina", "my-folder"],
            &["vmc", "usb-attach", "termina", "1:2"],
            &["vmc", "usb-detach", "termina", "5"],
            &["vmc", "usb-detach", "termina", "5"],
            &["vmc", "usb-list", "termina"],
            &["vmc", "--help"],
            &["vmc", "-h"],
        ];

        const DUMMY_FAILURE_ARGS: &[&[&str]] = &[
            &["vmc", "start"],
            &["vmc", "start", "--i-made-this-up", "termina"],
            &["vmc", "start", "termina", "extra args"],
            &["vmc", "stop"],
            &["vmc", "stop", "termina", "extra args"],
            &["vmc", "create"],
            &["vmc", "create", "-p"],
            &[
                "vmc",
                "create",
                "-p",
                "termina",
                "file name",
                "removable media",
                "extra args",
            ],
            &["vmc", "destroy"],
            &["vmc", "destroy", "termina", "extra args"],
            &["vmc", "disk-op-status"],
            &["vmc", "destroy", "12345", "extra args"],
            &["vmc", "export", "termina"],
            &["vmc", "export", "-d", "termina"],
            &["vmc", "export", "termina", "too", "many", "args"],
            &["vmc", "export", "-d", "termina", "too", "many", "args"],
            &["vmc", "import", "termina"],
            &["vmc", "import", "termina", "too", "many", "args"],
            &["vmc", "import", "-p", "termina"],
            &["vmc", "import", "-p", "termina", "too", "many", "args"],
            &["vmc", "list", "extra args"],
            &["vmc", "share"],
            &["vmc", "share", "too", "many", "args"],
            &["vmc", "unshare"],
            &["vmc", "unshare", "too", "many", "args"],
            &["vmc", "usb-attach"],
            &["vmc", "usb-attach", "termina"],
            &["vmc", "usb-attach", "termina", "whatever"],
            &["vmc", "usb-attach", "termina", "1:2:1dee:93d2"],
            &["vmc", "usb-detach"],
            &["vmc", "usb-detach", "not-a-number"],
            &["vmc", "usb-list"],
            &["vmc", "usb-list", "termina", "args"],
        ];

        let environ = vec![("CROS_USER_ID_HASH", "fake_hash")]
            .into_iter()
            .collect();
        for args in DUMMY_SUCCESS_ARGS {
            if let Err(e) = Vmc.run(&mut DummyDefaultBackend, args, &environ) {
                panic!("test args failed: {:?}: {}", args, e)
            }
        }
        for args in DUMMY_FAILURE_ARGS {
            if let Ok(()) = Vmc.run(&mut DummyDefaultBackend, args, &environ) {
                panic!("test args should have failed: {:?}", args)
            }
        }
    }

    #[test]
    fn container() {
        const CONTAINER_ARGS: &[&[&str]] = &[
            &["vmc", "container", "termina", "penguin"],
            &[
                "vmc",
                "container",
                "termina",
                "penguin",
                "https://my-image-server.com/",
                "custom/os",
            ],
        ];

        struct SessionsListBackend;

        impl Backend for SessionsListBackend {
            fn name(&self) -> &'static str {
                "Sessions List"
            }
            fn sessions_list(&mut self) -> Result<Vec<(String, String)>, Box<dyn Error>> {
                Ok(vec![(
                    "testuser@example.com".to_owned(),
                    "fake_hash".to_owned(),
                )])
            }
            fn vsh_exec_container(
                &mut self,
                _vm_name: &str,
                _user_id_hash: &str,
                _container_name: &str,
            ) -> Result<(), Box<dyn Error>> {
                Ok(())
            }
            fn container_create(
                &mut self,
                _vm_name: &str,
                _user_id_hash: &str,
                _container_name: &str,
                _source: ContainerSource,
            ) -> Result<(), Box<dyn Error>> {
                Ok(())
            }
            fn container_start(
                &mut self,
                _vm_name: &str,
                _user_id_hash: &str,
                _container_name: &str,
            ) -> Result<(), Box<dyn Error>> {
                Ok(())
            }
            fn container_setup_user(
                &mut self,
                _vm_name: &str,
                _user_id_hash: &str,
                _container_name: &str,
                _username: &str,
            ) -> Result<(), Box<dyn Error>> {
                Ok(())
            }
        }

        let environ = vec![("CROS_USER_ID_HASH", "fake_hash")]
            .into_iter()
            .collect();
        for args in CONTAINER_ARGS {
            if let Err(e) = Vmc.run(&mut SessionsListBackend, args, &environ) {
                panic!("test args failed: {:?}: {}", args, e)
            }
        }
    }
}
