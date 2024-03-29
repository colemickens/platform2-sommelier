// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod base;
mod dev;
mod dispatcher;
mod legacy;
mod util;

use std::env::var;
use std::io::{stdout, Write};
use std::mem;
use std::path::PathBuf;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicI32, Ordering};

use libc::{
    c_int, fork, kill, pid_t, sigaction, waitpid, SA_RESTART, SIGHUP, SIGINT, SIGKILL, SIG_DFL,
    WIFSTOPPED,
};

use rustyline::completion::Completer;
use rustyline::config::Configurer;
use rustyline::error::ReadlineError;
use rustyline::highlight::Highlighter;
use rustyline::hint::Hinter;
use rustyline::{CompletionType, Config, Context, Editor, Helper};

use sys_util::error;

use crate::dispatcher::{CompletionResult, Dispatcher};

const HISTORY_FILENAME: &str = ".crosh_history";

fn usage(error: bool) {
    let usage_msg = r#"Usage: crosh [options] [-- [args]]

Options:
  --dev         Force dev mode.
  --removable   Force removable (boot from USB/SD/etc...) mode.
  --usb         Same as above.
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

// Provides integration with rustyline.
struct ReadLineHelper {
    dispatcher: Dispatcher,
}

impl ReadLineHelper {
    fn dispatcher(&self) -> &Dispatcher {
        &self.dispatcher
    }
}

impl Helper for ReadLineHelper {}

impl Completer for ReadLineHelper {
    type Candidate = String;

    fn complete(
        &self,
        line: &str,
        _pos: usize,
        _ctx: &Context<'_>,
    ) -> Result<(usize, Vec<String>), ReadlineError> {
        let tokens: Vec<String> = parse_command(&line);
        match self.dispatcher.complete_command(tokens) {
            CompletionResult::NoMatches => Ok((0, vec![line.to_string(); 1])),
            CompletionResult::SingleDiff(diff) => Ok((line.len(), vec![diff; 1])),
            CompletionResult::WholeTokenList(matches) => Ok((0, matches)),
        }
    }
}

impl Hinter for ReadLineHelper {}

impl Highlighter for ReadLineHelper {}

// Forks off a child process which executes the command handler and waits for it to return.
// COMMAND_RUNNING_PID is updated to have the child process id so SIGINT can be sent.
fn handle_cmd(dispatcher: &Dispatcher, args: Vec<String>) -> Result<(), ()> {
    let pid: pid_t;
    unsafe {
        pid = fork();
    }
    if pid < 0 {
        return Err(());
    }
    // Handle the child thread case.
    if pid == 0 {
        clear_signal_handlers();
        dispatch_cmd(dispatcher, args);
    }

    COMMAND_RUNNING_PID.store(pid as i32, Ordering::Release);

    let mut status: c_int = 1;
    let code: pid_t;
    unsafe {
        code = waitpid(pid, &mut status, 0);
        // This should only happen if the child process is ptraced.
        if WIFSTOPPED(status) && kill(-pid, SIGKILL) != 0 {
            eprintln!("kill failed.");
        }
    }
    COMMAND_RUNNING_PID.store(-1, Ordering::Release);
    if code != pid {
        eprintln!("waitpid failed.");
        return Err(());
    }
    match status {
        0 => Ok(()),
        _ => Err(()),
    }
}

// Execute the specific command. This should be called in a child process.
fn dispatch_cmd(dispatcher: &Dispatcher, args: Vec<String>) {
    std::process::exit(match args.get(0).map(|s| &**s) {
        Some("help") => {
            let mut ret: i32 = 0;
            if args.len() == 2 {
                let list: [&str; 1] = [&args[1]];
                if dispatcher.help_string(&mut stdout(), Some(&list)).is_err() {
                    eprintln!("help: unknown command '{}'", &args[1]);
                    ret = 1;
                }
            } else {
                if args.len() > 1 {
                    eprintln!("ERROR: too many arguments");
                    ret = 1;
                }
                let list = ["exit", "help", "help_advanced", "ping", "top"];
                if dispatcher.help_string(&mut stdout(), Some(&list)).is_err() {
                    panic!();
                }
            }
            ret
        }
        Some("help_advanced") => {
            if args.len() > 1 {
                eprintln!("ERROR: too many arguments");
                1
            } else {
                if dispatcher.help_string(&mut stdout(), None).is_err() {
                    panic!();
                }
                0
            }
        }
        _ => match dispatcher.handle_command(args) {
            Ok(_) => 0,
            Err(e) => {
                eprintln!("ERROR: {}", e);
                1
            }
        },
    });
}

// Handle Ctrl-c/SIGINT by sending a SIGINT to any running child process.
unsafe extern "C" fn sigint_handler() {
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
extern "C" fn sighup_handler() {}

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

fn clear_signal_handlers() {
    unsafe {
        let mut sigact: sigaction = mem::zeroed();
        sigact.sa_sigaction = SIG_DFL;

        let ret = sigaction(SIGINT, &sigact, null_mut());
        if ret < 0 {
            eprintln!("sigaction failed for SIGINT.");
        }

        sigact = mem::zeroed();
        sigact.sa_sigaction = SIG_DFL;

        let ret = sigaction(SIGHUP, &sigact, null_mut());
        if ret < 0 {
            eprintln!("sigaction failed for SIGHUP.");
        }

        // Leave the handler for SIGTTIN.
    }
}

fn parse_command(command: &str) -> Vec<String> {
    // TODO(crbug.com/1002931) handle quotes
    command.split_whitespace().map(|s| s.to_string()).collect()
}

// Loop for getting each command from the user and dispatching it to the handler.
fn input_loop(dispatcher: Dispatcher) {
    let history_path = match var("HOME") {
        Ok(h) => {
            if h.is_empty() {
                None
            } else {
                Some(PathBuf::from(h).join(HISTORY_FILENAME))
            }
        }
        _ => None,
    };

    let helper = ReadLineHelper { dispatcher };

    let mut builder = Config::builder()
        .auto_add_history(true)
        .history_ignore_dups(true)
        .history_ignore_space(true)
        .completion_type(CompletionType::List);
    builder.set_max_history_size(4096);

    let config = builder.build();
    let mut editor = Editor::<ReadLineHelper>::with_config(config);
    editor.set_helper(Some(helper));

    if let Some(h) = history_path.as_ref() {
        if let Err(e) = editor.load_history(h) {
            eprintln!("Error loading history: {}", e);
        }
    }

    loop {
        match editor.readline("\x1b[1;33mcrosh>\x1b[0m ") {
            Ok(line) => {
                let command = line.trim();
                if command == "exit" || command == "quit" {
                    break;
                } else if !command.is_empty() {
                    let _ = handle_cmd(
                        editor.helper().unwrap().dispatcher(),
                        parse_command(&command),
                    );
                    if let Some(h) = history_path.as_ref() {
                        if let Err(e) = editor.save_history(h) {
                            eprintln!("Error persisting history: {}", e);
                        }
                    }
                }
            }
            Err(ReadlineError::Interrupted) => {
                // Ignore.
            }
            Err(ReadlineError::Eof) => {
                break;
            }
            Err(err) => {
                eprintln!("ReadLine error: {}", err);
                break;
            }
        }
    }
}

fn setup_dispatcher() -> Dispatcher {
    let mut dispatcher = Dispatcher::new();

    if util::dev_commands_included() {
        legacy::register_dev_mode_commands(&mut dispatcher);
        dev::register(&mut dispatcher);
    }

    if util::usb_commands_included() {
        legacy::register_removable_commands(&mut dispatcher);
    }

    base::register(&mut dispatcher);
    legacy::register(&mut dispatcher);

    if let Err(err) = dispatcher.validate() {
        panic!("FATAL: {}", err);
    }

    dispatcher
}

fn main() -> Result<(), ()> {
    let mut args = std::env::args();

    if args.next().is_none() {
        error!("expected executable name.");
        return Err(());
    }

    let mut args_as_command = false;

    let mut command_args: Vec<String> = Vec::new();

    util::set_dev_commands_included(util::is_dev_mode().unwrap_or_else(|_| {
        eprintln!("Could not locate 'crossystem'; assuming devmode is off.");
        false
    }));

    util::set_usb_commands_included(util::is_removable().unwrap_or_else(|_| {
        eprintln!("Could not query filesystem; assuming not removable.");
        false
    }));

    for arg in args {
        if args_as_command {
            command_args.push(arg)
        } else {
            match arg.as_ref() {
                "--help" | "-h" => {
                    usage(false);
                    return Ok(());
                }
                "--dev" => {
                    util::set_dev_commands_included(true);
                }
                "--removable" | "--usb" => {
                    util::set_usb_commands_included(true);
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

    let dispatcher = setup_dispatcher();

    if args_as_command {
        dispatch_cmd(
            &dispatcher,
            command_args.iter().map(|a| a.to_string()).collect(),
        );
        Ok(())
    } else {
        register_signal_handlers();

        intro();

        input_loop(dispatcher);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_registered_commands() {
        util::set_dev_commands_included(true);
        util::set_usb_commands_included(true);
        setup_dispatcher();
    }
}
