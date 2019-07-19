// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::io::{stdin, stdout, Write};
use std::mem;
use std::process::Command;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicI32, Ordering};
use std::thread::sleep;
use std::time::Duration;

use libc::{kill, sigaction, SA_RESTART, SIGHUP, SIGINT};
use sys_util::error;
use termion::event::Key;
use termion::input::TermRead;
use termion::raw::IntoRawMode;

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

// Handle Ctrl-c/SIGINT by sending a SIGINT to any running child process.
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

// Ignore SIGHUP.
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

// Handle user input to obtain a signal command. This includes handling cases like history lookups
// and command completion.
fn next_command() -> String {
    let mut stdin_keys = stdin().keys();
    new_prompt();
    let mut command = String::new();

    // Use the function scope to return stdout to normal mode before executing a command.
    let mut stdout = stdout().into_raw_mode().unwrap();
    loop {
        if let Some(Ok(key)) = stdin_keys.next() {
            match key {
                Key::Char('\t') => {
                    // TODO command completion
                }
                Key::Char('\n') => {
                    print!("\n\r");
                    if !command.is_empty() {
                        break;
                    } else {
                        new_prompt();
                    }
                }
                Key::Char(value) => {
                    print!("{}", value);
                    command.push(value);
                }
                Key::Backspace => {
                    if !command.is_empty() {
                        command.pop();
                        // Move cursor back one and clear rest of line.
                        print!("\x1b[D\x1b[K");
                    }
                }
                Key::Ctrl('d') => {
                    if command.is_empty() {
                        command = "exit".to_owned();
                        println!("\n\r");
                        break;
                    }
                }
                _ => {
                    // Ignore
                }
            }
            let _ = stdout.flush();
        } else {
            // If no input was returned, don't busy wait because stdin_keys.next() doesn't block.
            sleep(Duration::from_millis(25));
        }
    }
    let _ = stdout.flush();
    command
}

// Loop for getting each command from the user and dispatching it to the handler.
fn input_loop() {
    loop {
        let line = next_command();
        let command = line.trim();

        if command == "exit" || command == "quit" {
            break;
        } else if !command.is_empty() {
            handle_cmd(command.split_whitespace().collect())
                .unwrap_or_else(|_| println!("Command failed."));
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
                    return Ok(());
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

        input_loop();
        Ok(())
    }
}
