// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use libc::{kill, sigaction, SA_RESTART, SIGHUP, SIGINT};
use std::io::{stdin, stdout, Write};
use std::mem;
use std::process::Command;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicI32, Ordering};
use sys_util::error;

fn usage(error: bool) {
    let usage_msg = r#"
Usage: crosh [options] [-- [args]]

Options:
  --help, -h    Show this help string.
  -- <all args after this are a command to run>
                Execute a single command and exit.
"#;
    if error {
        eprintln!("{}", usage_msg)
    } else {
        println!("{}", usage_msg);
    }
}

fn intro() {
    println!(
        r#"Welcome to crosh, the Chrome OS developer shell.

If you got here by mistake, don't panic!  Just close this tab and carry on.

Type 'help' for a list of commands.

If you want to customize the look/behavior, you can use the options page.
Load it by using the Ctrl-Shift-P keyboard shortcut.
"#
    );
}

fn new_prompt() {
    print!("\x1b[1;33mcrosh>\x1b[0m ");
    let _ = stdout().flush();
}

static COMMAND_RUNNING_PID: AtomicI32 = AtomicI32::new(-1);

fn handle_cmd(args: Vec<&str>) -> Result<(), ()> {
    if args.len() < 1 {
        return Err(());
    }
    let result = match Command::new("crosh.sh").arg("--").args(args).spawn() {
        Ok(mut child) => {
            let pid: u32 = child.id();
            if pid > std::i32::MAX as u32 {
                return Err(());
            }
            COMMAND_RUNNING_PID.store(pid as i32, Ordering::Release);
            match child.wait().map(|a| a.success()) {
                Ok(true) => Ok(()),
                _ => Err(()),
            }
        }
        _ => Err(()),
    };
    COMMAND_RUNNING_PID.store(-1, Ordering::Relaxed);
    result
}

unsafe extern "C" fn sigint_handler() -> () {
    let mut command_pid: i32 = COMMAND_RUNNING_PID.load(Ordering::Acquire);
    if command_pid >= 0 {
        let _ = stdout().flush();
        if kill(command_pid, SIGINT) != 0 {
            eprintln!("kill failed.");
        } else {
            command_pid = -1;
        }
    } else {
        println!();
        new_prompt();
    }
    COMMAND_RUNNING_PID.store(command_pid, Ordering::Release);
}

extern "C" fn sighup_handler() -> () {}

fn register_signal_handlers() {
    unsafe {
        let mut sigact: sigaction = mem::zeroed();
        sigact.sa_flags = SA_RESTART;
        sigact.sa_sigaction = sigint_handler as *const () as usize;

        let ret = sigaction(SIGINT, &sigact, null_mut());
        if ret < 0 {
            eprintln!("sigaction failed for SIGINT.");
        }

        sigact = mem::zeroed();
        sigact.sa_flags = SA_RESTART;
        sigact.sa_sigaction = sighup_handler as *const () as usize;

        let ret = sigaction(SIGHUP, &sigact, null_mut());
        if ret < 0 {
            eprintln!("sigaction failed for SIGHUP.");
        }
    }
}

fn main() -> Result<(), ()> {
    let mut args = std::env::args();

    if args.next().is_none() {
        error!("expected executable name.");
        return Err(());
    }

    let mut args_as_command = false;

    let mut command_args: Vec<String> = Vec::new();

    for arg in args {
        if args_as_command {
            command_args.push(arg)
        } else {
            match arg.as_ref() {
                "--help" | "-h" => {
                    usage(false);
                }
                "--" => {
                    args_as_command = true;
                }
                _ => {
                    usage(true);
                    return Err(());
                }
            }
        }
    }
    if args_as_command {
        handle_cmd(command_args.iter().map(|a| &**a).collect())
    } else {
        register_signal_handlers();

        intro();

        loop {
            new_prompt();

            let mut line = String::new();
            stdin().read_line(&mut line).expect("Error getting stdin.");
            let command = line.trim();

            if command == "exit" || command == "quit" {
                break;
            } else if !command.is_empty() {
                if handle_cmd(command.split_whitespace().collect()).is_err() {
                    println!("Command failed.");
                }
            }
        }
        Ok(())
    }
}
