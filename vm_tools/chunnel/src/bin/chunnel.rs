// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::env;
use std::ffi::CStr;
use std::fmt;
use std::process;
use std::result;

use getopts::Options;
use libchromeos::syslog;
use log::warn;
use sys_util::{self, block_signal, PollContext, PollToken};

use chunnel::forwarder::{ForwarderError, ForwarderSession};
use chunnel::stream::{StreamSocket, StreamSocketError};

// Program name.
const IDENT: &[u8] = b"chunnel\0";

#[remain::sorted]
#[derive(Debug)]
enum Error {
    BlockSigpipe(sys_util::signal::Error),
    ConnectSocket(StreamSocketError),
    Forward(ForwarderError),
    PollContextDelete(sys_util::Error),
    PollContextNew(sys_util::Error),
    PollWait(sys_util::Error),
    Syslog(log::SetLoggerError),
}

type Result<T> = result::Result<T, Error>;

impl fmt::Display for Error {
    #[remain::check]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Error::*;

        #[remain::sorted]
        match self {
            BlockSigpipe(e) => write!(f, "failed to block SIGPIPE: {}", e),
            ConnectSocket(e) => write!(f, "failed to connnect socket: {}", e),
            Forward(e) => write!(f, "failed to forward traffic: {}", e),
            PollContextDelete(e) => write!(f, "failed to delete fd from poll context: {}", e),
            PollContextNew(e) => write!(f, "failed to create poll context: {}", e),
            PollWait(e) => write!(f, "failed to wait for poll: {}", e),
            Syslog(e) => write!(f, "failed to initialize syslog: {}", e),
        }
    }
}

fn print_usage(program: &str, opts: &Options) {
    let brief = format!("Usage: {} [options]", program);
    print!("{}", opts.usage(&brief));
}

fn run_forwarder(local_stream: StreamSocket, remote_stream: StreamSocket) -> Result<()> {
    block_signal(libc::SIGPIPE).map_err(Error::BlockSigpipe)?;

    #[derive(PollToken)]
    enum Token {
        LocalStreamReadable,
        RemoteStreamReadable,
    }
    let poll_ctx: PollContext<Token> = PollContext::build_with(&[
        (&local_stream, Token::LocalStreamReadable),
        (&remote_stream, Token::RemoteStreamReadable),
    ])
    .map_err(Error::PollContextNew)?;

    let mut forwarder = ForwarderSession::new(local_stream, remote_stream);

    loop {
        let events = poll_ctx.wait().map_err(Error::PollWait)?;

        for event in events.iter_readable() {
            match event.token() {
                Token::LocalStreamReadable => {
                    let shutdown = forwarder.forward_from_local().map_err(Error::Forward)?;
                    if shutdown {
                        poll_ctx
                            .delete(forwarder.local_stream())
                            .map_err(Error::PollContextDelete)?;
                    }
                }
                Token::RemoteStreamReadable => {
                    let shutdown = forwarder.forward_from_remote().map_err(Error::Forward)?;
                    if shutdown {
                        poll_ctx
                            .delete(forwarder.remote_stream())
                            .map_err(Error::PollContextDelete)?;
                    }
                }
            }
        }
        if forwarder.is_shut_down() {
            return Ok(());
        }
    }
}

fn main() -> Result<()> {
    let args: Vec<String> = env::args().collect();
    let program = args[0].clone();

    let mut opts = Options::new();
    opts.optflag("h", "help", "print this help menu");
    opts.reqopt("l", "local", "local socket to forward", "SOCKADDR");
    opts.reqopt("r", "remote", "remote socket to forward to", "SOCKADDR");
    opts.optopt("t", "type", "type of traffic to forward", "stream|datagram");

    let matches = match opts.parse(&args[1..]) {
        Ok(m) => m,
        Err(e) => {
            warn!("failed to parse arg: {}", e);
            print_usage(&program, &opts);
            process::exit(1);
        }
    };
    if matches.opt_present("h") {
        print_usage(&program, &opts);
        return Ok(());
    }

    // Safe because this string is defined above in this file and it contains exactly
    // one nul byte, which appears at the end.
    let ident = CStr::from_bytes_with_nul(IDENT).unwrap();
    syslog::init(ident).map_err(Error::Syslog)?;

    let local_sockaddr = match matches.opt_str("l") {
        Some(sockaddr) => sockaddr,
        None => {
            warn!("local socket must be defined");
            print_usage(&program, &opts);
            process::exit(1);
        }
    };

    let remote_sockaddr = match matches.opt_str("r") {
        Some(sockaddr) => sockaddr,
        None => {
            warn!("remote socket must be defined");
            print_usage(&program, &opts);
            process::exit(1);
        }
    };

    // Default to "stream" if traffic type is not defined.
    let traffic_type = matches.opt_str("t");
    if let Some(t) = traffic_type {
        match t.as_ref() {
            "stream" => {}
            "datagram" => {
                warn!("datagram sockets are not yet supported");
                process::exit(1);
            }
            s => {
                warn!("not a valid type of traffic: {}", s);
                print_usage(&program, &opts);
                process::exit(1);
            }
        }
    }

    let local_stream = StreamSocket::connect(&local_sockaddr).map_err(Error::ConnectSocket)?;
    let remote_stream = StreamSocket::connect(&remote_sockaddr).map_err(Error::ConnectSocket)?;

    run_forwarder(local_stream, remote_stream)
}
