// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// Runs a [9P] server.
///
/// [9P]: http://man.cat-v.org/plan_9/5/0intro
extern crate getopts;
extern crate libc;
#[macro_use]
extern crate log;
extern crate p9;

mod syslog;
mod vsock;

use libc::gid_t;

use std::ffi::{CStr, CString};
use std::fmt;
use std::fs::remove_file;
use std::io::{self, BufReader, BufWriter};
use std::net;
use std::num::ParseIntError;
use std::os::raw::c_uint;
use std::os::unix::fs::FileTypeExt;
use std::os::unix::fs::PermissionsExt;
use std::os::unix::net::{SocketAddr, UnixListener};
use std::path::{Path, PathBuf};
use std::result;
use std::str::FromStr;
use std::string;
use std::sync::Arc;
use std::thread;

use vsock::*;

const DEFAULT_BUFFER_SIZE: usize = 8192;

// Address family identifiers.
const VSOCK: &'static str = "vsock:";
const UNIX: &'static str = "unix:";

// Usage for this program.
const USAGE: &'static str = "9s [options] {vsock:<port>|unix:<path>|<ip>:<port>}";

// Program name.
const IDENT: &'static [u8] = b"9s\0";

enum ListenAddress {
    Net(net::SocketAddr),
    Unix(String),
    Vsock(c_uint),
}

#[derive(Debug)]
enum ParseAddressError {
    MissingUnixPath,
    MissingVsockPort,
    Net(net::AddrParseError),
    Unix(string::ParseError),
    Vsock(ParseIntError),
}

impl fmt::Display for ParseAddressError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            &ParseAddressError::MissingUnixPath => write!(f, "missing unix path"),
            &ParseAddressError::MissingVsockPort => write!(f, "missing vsock port number"),
            &ParseAddressError::Net(ref e) => e.fmt(f),
            &ParseAddressError::Unix(ref e) => write!(f, "invalid unix path: {}", e),
            &ParseAddressError::Vsock(ref e) => write!(f, "invalid vsock port number: {}", e),
        }
    }
}

impl FromStr for ListenAddress {
    type Err = ParseAddressError;

    fn from_str(s: &str) -> result::Result<Self, Self::Err> {
        if s.starts_with(VSOCK) {
            if s.len() > VSOCK.len() {
                Ok(ListenAddress::Vsock(
                    s[VSOCK.len()..].parse().map_err(ParseAddressError::Vsock)?,
                ))
            } else {
                Err(ParseAddressError::MissingVsockPort)
            }
        } else if s.starts_with(UNIX) {
            if s.len() > UNIX.len() {
                Ok(ListenAddress::Unix(
                    s[UNIX.len()..].parse().map_err(ParseAddressError::Unix)?,
                ))
            } else {
                Err(ParseAddressError::MissingUnixPath)
            }
        } else {
            Ok(ListenAddress::Net(
                s.parse().map_err(ParseAddressError::Net)?,
            ))
        }
    }
}

#[derive(Debug)]
enum Error {
    Address(ParseAddressError),
    Argument(getopts::Fail),
    Cid(ParseIntError),
    IO(io::Error),
    MissingAcceptCid,
    SocketGid(ParseIntError),
    SocketPathNotAbsolute(PathBuf),
    Syslog(log::SetLoggerError),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            &Error::Address(ref e) => e.fmt(f),
            &Error::Argument(ref e) => e.fmt(f),
            &Error::Cid(ref e) => write!(f, "invalid cid value: {}", e),
            &Error::IO(ref e) => e.fmt(f),
            &Error::MissingAcceptCid => write!(f, "`accept_cid` is required for vsock servers"),
            &Error::SocketGid(ref e) => write!(f, "invalid gid value: {}", e),
            &Error::SocketPathNotAbsolute(ref p) => {
                write!(f, "unix socket path must be absolute: {:?}", p)
            }
            &Error::Syslog(ref e) => write!(f, "failed to initialize syslog: {}", e),
        }
    }
}

struct UnixSocketAddr(SocketAddr);
impl fmt::Display for UnixSocketAddr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if let Some(path) = self.0.as_pathname() {
            write!(f, "{}", path.to_str().unwrap_or("<malformed path>"))
        } else {
            write!(f, "<unnamed or abstract socket>")
        }
    }
}

type Result<T> = result::Result<T, Error>;

fn handle_client<R: io::Read, W: io::Write>(
    root: Arc<str>,
    mut reader: R,
    mut writer: W,
) -> io::Result<()> {
    let mut server = p9::Server::new(&*root);

    loop {
        server.handle_message(&mut reader, &mut writer)?;
    }
}

fn spawn_server_thread<
    R: 'static + io::Read + Send,
    W: 'static + io::Write + Send,
    D: 'static + fmt::Display + Send,
>(
    root: &Arc<str>,
    reader: R,
    writer: W,
    peer: D,
) {
    let reader = BufReader::with_capacity(DEFAULT_BUFFER_SIZE, reader);
    let writer = BufWriter::with_capacity(DEFAULT_BUFFER_SIZE, writer);
    let server_root = root.clone();
    thread::spawn(move || {
        if let Err(e) = handle_client(server_root, reader, writer) {
            error!("error while handling client {}: {}", peer, e);
        }
    });
}

fn run_vsock_server(root: Arc<str>, port: c_uint, accept_cid: c_uint) -> io::Result<()> {
    let listener = VsockListener::bind(port)?;

    loop {
        let (stream, peer) = listener.accept()?;

        if accept_cid != peer.cid {
            warn!("ignoring connection from {}", peer);
            continue;
        }

        info!("accepted connection from {}", peer);
        spawn_server_thread(&root, stream.try_clone()?, stream, peer);
    }
}

fn adjust_socket_ownership(path: &Path, gid: gid_t) -> io::Result<()> {
    // At this point we expect valid path since we supposedly created
    // the socket, so any failure in transforming path is _really_ unexpected.
    let path_str = path.as_os_str().to_str().expect("invalid unix socket path");
    let path_cstr = CString::new(path_str).expect("malformed unix socket path");

    // Safe as kernel only reads from the path and we know it is properly
    // formed and we check the result for errors.
    // Note: calling chown with uid -1 will preserve current user ownership.
    let res = unsafe { libc::chown(path_cstr.as_ptr(), libc::uid_t::max_value(), gid) };
    if res < 0 {
        return Err(io::Error::last_os_error());
    }

    // Allow both owner and group read/write access to the socket, and
    // deny access to the rest of the world.
    let mut permissions = path.metadata()?.permissions();
    permissions.set_mode(0o660);

    Ok(())
}

fn run_unix_server(root: Arc<str>, path: &Path, socket_gid: Option<gid_t>) -> io::Result<()> {
    if path.exists() {
        let metadata = path.metadata()?;
        if !metadata.file_type().is_socket() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Requested socked path points to existing non-socket object",
            ));
        }
        remove_file(path)?;
    }

    let listener = UnixListener::bind(path)?;

    if let Some(gid) = socket_gid {
        adjust_socket_ownership(path, gid)?;
    }

    loop {
        let (stream, peer) = listener.accept()?;
        let peer = UnixSocketAddr(peer);

        info!("accepted connection from {}", peer);
        spawn_server_thread(&root, stream.try_clone()?, stream, peer);
    }
}

fn main() -> Result<()> {
    let mut opts = getopts::Options::new();
    opts.optopt(
        "",
        "accept_cid",
        "only accept connections from this vsock context id",
        "CID",
    );
    opts.optopt(
        "r",
        "root",
        "root directory for clients (default is \"/\")",
        "PATH",
    );
    opts.optopt(
        "",
        "socket_gid",
        "change socket group ownership to the specified ID",
        "GID",
    );
    opts.optflag("h", "help", "print this help menu");

    let matches = opts
        .parse(std::env::args_os().skip(1))
        .map_err(Error::Argument)?;

    if matches.opt_present("h") || matches.free.len() == 0 {
        print!("{}", opts.usage(USAGE));
        return Ok(());
    }

    let root: Arc<str> = Arc::from(matches.opt_str("r").unwrap_or_else(|| "/".into()));

    // Safe because this string is defined above in this file and it contains exactly
    // one nul byte, which appears at the end.
    let ident = CStr::from_bytes_with_nul(IDENT).unwrap();
    syslog::init(ident).map_err(Error::Syslog)?;

    // We already checked that |matches.free| has at least one item.
    match matches.free[0]
        .parse::<ListenAddress>()
        .map_err(Error::Address)?
    {
        ListenAddress::Vsock(port) => {
            let accept_cid = if let Some(cid) = matches.opt_str("accept_cid") {
                cid.parse::<c_uint>().map_err(Error::Cid)
            } else {
                Err(Error::MissingAcceptCid)
            }?;
            run_vsock_server(root, port, accept_cid).map_err(Error::IO)?;
        }
        ListenAddress::Net(_) => {
            error!("Network server unimplemented");
        }
        ListenAddress::Unix(path) => {
            let path = Path::new(&path);
            if !path.is_absolute() {
                return Err(Error::SocketPathNotAbsolute(path.to_owned()));
            }
            let socket_gid = matches
                .opt_get::<gid_t>("socket_gid")
                .map_err(Error::SocketGid)?;
            run_unix_server(root, path, socket_gid).map_err(Error::IO)?;
        }
    }

    Ok(())
}
