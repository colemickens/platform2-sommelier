// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This program collects fine-grained memory stats around events of interest
// (such as browser tab discards) and saves them in a queue of "clip files",
// to be uploaded with other logs.
//
// The program has two modes: slow and fast poll.  In slow-poll mode, the
// program occasionally checks (every 2 seconds right now) whether it should go
// into fast-poll mode, because interesting events become possible.  When in
// fast-poll mode, the program collects memory stats frequently (every 0.1
// seconds right now) and stores them in a circular buffer.  When "interesting"
// events occur, the stats around each event are saved into a "clip" file.
// These files are also maintained as a queue, so only the latest N clips are
// available (N = 20 right now).

extern crate chrono;
extern crate libc;
extern crate dbus;

#[macro_use] extern crate log;
extern crate env_logger;
extern crate syslog;

use chrono::prelude::*;
use {libc::__errno_location, libc::c_void};

use dbus::{Connection, BusType, WatchEvent};

use std::{io,mem,str,thread};
use std::cmp::max;
use std::error::Error;
use std::fmt;
use std::fs::{create_dir, File, OpenOptions};
use std::io::prelude::*;
use std::os::unix::fs::OpenOptionsExt;
use std::os::unix::io::{AsRawFd, RawFd};
use std::path::{Path, PathBuf};
// Not to be confused with chrono::Duration or the deprecated time::Duration.
use std::time::Duration;

const PAGE_SIZE: usize =            4096;   // old friend

const LOG_DIRECTORY: &str =         "/var/log/memd";
const STATIC_PARAMETERS_LOG: &str = "memd.parameters";
const LOW_MEM_SYSFS: &str =         "/sys/kernel/mm/chromeos-low_mem";
const TRACING_SYSFS: &str =         "/sys/kernel/debug/tracing";
const LOW_MEM_DEVICE: &str =        "/dev/chromeos-low-mem";
const MAX_CLIP_COUNT: usize =       20;

const COLLECTION_DELAY_MS: i64 = 5_000;       // Wait after event of interest.
const CLIP_COLLECTION_SPAN_MS: i64 = 10_000;  // ms worth of samples in a clip.
const SAMPLES_PER_SECOND: i64 = 10;           // Rate of fast sample collection.
const SAMPLING_PERIOD_MS: i64 = 1000 / SAMPLES_PER_SECOND;
const LOW_MEM_SAFETY_FACTOR: u32 = 3;         // Low-mem margin multiplier for polling.
const SLOW_POLL_PERIOD_DURATION: Duration = Duration::from_secs(2); // Sleep in slow-poll mode.
const FAST_POLL_PERIOD_DURATION: Duration =
    Duration::from_millis(SAMPLING_PERIOD_MS as u64); // Sleep duration in fast-poll mode.

// Print a warning if the fast-poll select lasts a lot longer than expected
// (which might happen because of huge load average and swap activity).
const UNREASONABLY_LONG_SLEEP: i64 = 10 * SAMPLING_PERIOD_MS;

// Size of sample queue.  The queue contains mostly timer events, in the amount
// determined by the clip span and the sampling rate.  It also contains other
// events, such as OOM kills etc., whose amount is expected to be smaller than
// the former.  Generously double the number of timer events to leave room for
// non-timer events.
const SAMPLE_QUEUE_LENGTH: usize =
    (CLIP_COLLECTION_SPAN_MS / 1000 * SAMPLES_PER_SECOND * 2) as usize;

// For testing: different levels of available RAM in MB.
const TESTING_LOW_MEM_LOW_AVAILABLE: usize = 150;
const TESTING_LOW_MEM_MEDIUM_AVAILABLE: usize = 300;
const TESTING_LOW_MEM_HIGH_AVAILABLE: usize = 1000;
const TESTING_LOW_MEM_MARGIN: usize = 200;
const TESTING_MOCK_DBUS_FIFO_NAME: &str = "mock-dbus-fifo";

// The names of fields of interest in /proc/vmstat.  They must be listed in
// the order in which they appear in /proc/vmstat.  When parsing the file,
// if a mandatory field is missing, the program panics.  A missing optional
// field (for instance, pgmajfault_f for some kernels) results in a value
// of 0.
const VMSTAT_VALUES_COUNT: usize = 7;           // Number of vmstat values we're tracking.
#[cfg(target_arch = "x86_64")]
const VMSTATS: [(&str, bool); VMSTAT_VALUES_COUNT] = [
    // name                     mandatory
    ("nr_pages_scanned",        true),
    ("pswpin",                  true),
    ("pswpout",                 true),
    ("pgalloc_dma32",           true),
    ("pgalloc_normal",          true),
    ("pgmajfault",              true),
    ("pgmajfault_f",            false),
];
#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
// The only difference from x86_64 is pgalloc_dma vs. pgalloc_dma32.
// Wish there was some more concise mechanism.
const VMSTATS: [(&str, bool); VMSTAT_VALUES_COUNT] = [
    // name                     mandatory
    ("nr_pages_scanned",        true),
    ("pswpin",                  true),
    ("pswpout",                 true),
    ("pgalloc_dma",             true),
    ("pgalloc_normal",          true),
    ("pgmajfault",              true),
    ("pgmajfault_f",            false),
];

macro_rules! print_to_path {
    ($path:expr, $format:expr $(, $arg:expr)*) => {{
        let r = OpenOptions::new().write(true).create(true).open($path);
        match r {
            Err(e) => Err(e),
            Ok(mut f) => f.write_all(format!($format $(, $arg)*).as_bytes())
        }
    }}
}

type Result<T> = std::result::Result<T, Box<Error>>;

fn errno() -> i32 {
    // _errno_location() is trivially safe to call and dereferencing the
    // returned pointer is always safe because it's guaranteed to be valid and
    // point to thread-local data.  (Thanks to Zach for this comment.)
    unsafe { *__errno_location() }
}

// Returns the string for posix error number |err|.
fn strerror(err: i32) -> String {
    // safe to leave |buffer| uninitialized because we initialize it from strerror_r.
    let mut buffer: [u8; PAGE_SIZE] = unsafe { mem::uninitialized() };
    // |err| is valid because it is the libc errno at all call sites.  |buffer|
    // is converted to a valid address, and |buffer.len()| is a valid length.
    let n = unsafe {
        libc::strerror_r(err, &mut buffer[0] as *mut u8 as *mut libc::c_char, buffer.len())
    } as usize;
    match str::from_utf8(&buffer[..n]) {
        Ok(s) => s.to_string(),
        Err(e) => {
            // Ouch---an error while trying to process another error.
            // Very unlikely so we just panic.
            panic!("error {:?} converting {:?}", e, &buffer[..n]);
        }
    }
}

// Opens a file.  Returns an error which includes the file name.
fn open(path: &Path) -> Result<File> {
    Ok(File::open(path).map_err(|e| format!("{:?}: {}", path, e))?)
}

// Opens a file if it exists, otherwise returns none.
fn open_maybe(path: &Path) -> Result<Option<File>> {
    if !path.exists() {
        Ok(None)
    } else {
        Ok(Some(open(path)?))
    }
}

// Opens a file with mode flags (unfortunately named OpenOptions).
fn open_with_flags(path: &Path, options: &OpenOptions) -> Result<File> {
    Ok(options.open(path).map_err(|e| format!("{:?}: {}", path, e))?)
}

// Opens a file with mode flags.  If the file does not exist, returns None.
fn open_with_flags_maybe(path: &Path, options: &OpenOptions) -> Result<Option<File>> {
    if !path.exists() {
        Ok(None)
    } else {
        Ok(Some(open_with_flags(&path, &options)?))
    }
}

fn mkfifo(path: &PathBuf) -> Result<()> {
    let path_name = path.to_str().unwrap();
    let c_path = std::ffi::CString::new(path_name).unwrap();
    // Safe because c_path points to a valid C string.
    let status = unsafe { libc::mkfifo(c_path.as_ptr(), 0o644) };
    if status < 0 {
        Err(format!("mkfifo: {}: {}", path_name, strerror(errno())).into())
    } else {
        Ok(())
    }
}

// Converts the result of an integer expression |e| to modulo |n|. |e| may be
// negative. This differs from plain "%" in that the result of this function
// is always be between 0 and n-1.
fn modulo(e: isize, n: usize) -> usize {
    let nn = n as isize;
    let x = e % nn;
    (if x >= 0 { x } else { x + nn }) as usize
}

fn duration_to_millis(duration: &Duration) -> i64 {
    duration.as_secs() as i64 * 1000 + duration.subsec_nanos() as i64 / 1_000_000
}

// Reads a string from the file named by |path|, representing a u32, and
// returns the value the strings represents.
fn read_int(path: &Path) -> Result<u32> {
    let mut file = open(path)?;
    let mut content = String::new();
    file.read_to_string(&mut content)?;
    Ok(content.trim().parse::<u32>()?)
}

// Internally generated event for testing.
struct TestEvent {
    time: i64,
    event_type: TestEventType,
}

// The types of events which are generated internally for testing.  They
// simulate state changes (for instance, change in the memory pressure level),
// chrome events, and kernel events.
enum TestEventType {
    EnterHighPressure,    // enter low available RAM (below margin) state
    EnterLowPressure,     // enter high available RAM state
    EnterMediumPressure,  // set enough memory pressure to trigger fast sampling
    OomKillBrowser,       // fake browser report of OOM kill
    OomKillKernel,        // fake kernel report of OOM kill
    TabDiscard,           // fake browser report of tab discard
}

impl TestEvent {
    fn deliver(&self, paths: &Paths, dbus_fifo: &mut File, low_mem_device: &mut File) {
        match self.event_type {
            TestEventType::EnterLowPressure =>
                self.low_mem_notify(TESTING_LOW_MEM_HIGH_AVAILABLE, &paths, low_mem_device),
            TestEventType::EnterMediumPressure =>
                self.low_mem_notify(TESTING_LOW_MEM_MEDIUM_AVAILABLE, &paths, low_mem_device),
            TestEventType::EnterHighPressure =>
                self.low_mem_notify(TESTING_LOW_MEM_LOW_AVAILABLE, &paths, low_mem_device),
            TestEventType::OomKillBrowser => self.send_signal("oom-kill", dbus_fifo),
            TestEventType::OomKillKernel => self.mock_kill(&paths.trace_pipe),
            TestEventType::TabDiscard => self.send_signal("tab-discard", dbus_fifo),
        }
    }

    fn low_mem_notify(&self, amount: usize, paths: &Paths, mut low_mem_device: &mut File) {
        write_string(&amount.to_string(), &paths.available, false).
            expect("available file: write failed");
        if amount == TESTING_LOW_MEM_LOW_AVAILABLE {
            // Make low-mem device ready-to-read.
            write!(low_mem_device, ".").expect("low-mem-device: write failed");
        } else {
            let mut buf = [0; PAGE_SIZE];
            read_nonblocking_pipe(&mut low_mem_device, &mut buf)
                .expect("low-mem-device: clear failed");
        }
    }

    fn send_signal(&self, signal: &str, dbus_fifo: &mut File) {
        write!(dbus_fifo, "{}\n", signal).expect("mock dbus: write failed");
    }

    fn mock_kill(&self, path: &PathBuf) {
        // example string (8 spaces before the first non-space):
        // chrome-13700 [001] .... 867348.061651: oom_kill_process <-out_of_memory
        let s = format!("chrome-13700 [001] .... {}.{:06}: oom_kill_process <-out_of_memory\n",
                        self.time / 1000, (self.time % 1000) * 1000);
        write_string(&s, path, true).expect("mock trace_pipe: write failed");
    }
}

fn non_blocking_select(high_fd: i32, inout_read_fds: &mut libc::fd_set) -> i32 {
    let mut null_timeout = libc::timeval {
        tv_sec: 0 as libc::time_t,
        tv_usec: 0 as libc::suseconds_t,
    };
    let null = std::ptr::null_mut();
    // Safe because we're passing valid values and addresses.
    unsafe {
        libc::select(high_fd,
                     inout_read_fds,
                     null,
                     null,
                     &mut null_timeout as *mut libc::timeval)
    }
}

// The Timer trait allows us to mock time for testing.
trait Timer {
    // A wrapper for libc::select() with only ready-to-read fds.
    fn select(&mut self, nfds:
              libc::c_int,
              fds: &mut libc::fd_set,
              timeout: &Duration) -> libc::c_int;
    fn sleep(&mut self, sleep_time: &Duration);
    fn now(&self) -> i64;
    fn quit_request(&self) -> bool;
}

// Real time mock, for testing only.  It removes time races (for better or
// worse) and makes it possible to run the test on build machines which may be
// heavily loaded.
//
// Time is mocked by assuming that CPU speed is infinite and time passes only
// when the program is asleep. Time advances in discrete jumps when we call
// either sleep() or select() with a timeout.

struct MockTimer {
    current_time: i64,         // the current time
    test_events: Vec<TestEvent>,  // list events to be delivered
    event_index: usize,        // index of next event to be delivered
    paths: Paths,              // for event delivery
    dbus_fifo_out: File,       // for mock dbus event delivery
    low_mem_device: File,      // for delivery of low-mem notifications
    quit_request: bool,        // for termination
    _trace_pipe_aux: File,     // to avoid EOF on trace pipe, which fires select
}

impl MockTimer {
    fn new(test_events: Vec<TestEvent>, paths: Paths, dbus_fifo_out: File) -> MockTimer {
        let low_mem_device = OpenOptions::new()
            .custom_flags(libc::O_NONBLOCK)
            .read(true)
            .write(true)
            .open(&paths.low_mem_device).expect("low-mem-device: cannot setup");
        // We need to open the fifo one extra time because otherwise the fifo
        // is still ready-to-read after consuming all data in its buffer, even
        // though the following read will return EOF.  The ready-to-read status
        // will cause the select() to fire incorrectly.  By having another open
        // write descriptor, there is no EOF, and the select does not fire.
        let _trace_pipe_aux = OpenOptions::new()
            .custom_flags(libc::O_NONBLOCK)
            .read(true)
            .write(true)
            .open(&paths.trace_pipe).expect("trace_pipe: cannot setup");
        MockTimer {
            current_time: 0,
            test_events,
            event_index: 0,
            paths,
            dbus_fifo_out,
            low_mem_device,
            quit_request: false,
            _trace_pipe_aux,
        }
    }
}

impl Timer for MockTimer {
    fn now(&self) -> i64 {
        self.current_time
    }

    fn quit_request(&self) -> bool {
        self.quit_request
    }

    // Mock select first checks if any events are pending, then produces events
    // that would happen during its sleeping time, and checks if those events
    // are delivered.
    fn select(&mut self,
              high_fd: i32,
              inout_read_fds: &mut libc::fd_set,
              timeout: &Duration) -> i32 {
        // First check for pending events which may have been delivered during
        // a preceding sleep.
        let n = non_blocking_select(high_fd, inout_read_fds);
        if n != 0 {
            return n;
        }
        // Now time can start passing, as long as there are more events.
        let timeout_ms = duration_to_millis(&timeout);
        let end_time = self.current_time + timeout_ms;
        if self.event_index == self.test_events.len() {
            // No more events to deliver, so no need for further select() calls.
            self.quit_request = true;
            self.current_time = end_time;
            return 0;
        }
        // There are still event to be delivered.
        let first_event_time = self.test_events[self.event_index].time;
        if first_event_time > end_time {
            // No event to deliver before the timeout, thus no events to look for.
            self.current_time = end_time;
            return 0;
        }
        // Deliver all events with the time stamp of the first event.  (There
        // is at least one.)
        while {
            self.test_events[self.event_index].deliver(&self.paths,
                                                       &mut self.dbus_fifo_out,
                                                       &mut self.low_mem_device);
            self.event_index += 1;
            self.event_index < self.test_events.len() &&
            self.test_events[self.event_index].time == first_event_time
        } {}
        // One or more events were delivered, so run another select.
        let n = non_blocking_select(high_fd, inout_read_fds);
        if n > 0 {
            self.current_time = first_event_time;
        } else {
            self.current_time = end_time;
        }
        n
    }

    // Mock sleep produces all events that would happen during that sleep, then
    // updates the time.
    fn sleep(&mut self, sleep_time: &Duration) {
        let start_time = self.current_time;
        let end_time = start_time + duration_to_millis(&sleep_time);
        while self.event_index < self.test_events.len() &&
            self.test_events[self.event_index].time <= end_time {
                self.test_events[self.event_index].deliver(&self.paths,
                                                           &mut self.dbus_fifo_out,
                                                           &mut self.low_mem_device);
            self.event_index += 1;
        }
        if self.event_index == self.test_events.len() {
            self.quit_request = true;
        }
        self.current_time = end_time;
    }
}

struct GenuineTimer {}

impl Timer for GenuineTimer {
    // Returns current uptime (active time since boot, in milliseconds)
    fn now(&self) -> i64 {
        let mut ts: libc::timespec;
        // clock_gettime is safe when passed a valid address and a valid enum.
        let result = unsafe {
            ts = mem::uninitialized();
            libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut ts as *mut libc::timespec)
        };
        assert_eq!(result, 0, "clock_gettime() failed!");
        ts.tv_sec as i64 * 1000 + ts.tv_nsec as i64 / 1_000_000
    }

    // Always returns false, unless testing (see MockTimer).
    fn quit_request(&self) -> bool {
        false
    }

    fn sleep(&mut self, sleep_time: &Duration) {
        thread::sleep(*sleep_time);
    }

    fn select(&mut self,
              high_fd: i32,
              inout_read_fds: &mut libc::fd_set,
              timeout: &Duration) -> i32 {
        let mut libc_timeout = libc::timeval {
            tv_sec: timeout.as_secs() as libc::time_t,
            tv_usec: (timeout.subsec_nanos() / 1000) as libc::suseconds_t,
        };
        let null = std::ptr::null_mut();
        // Safe because we're passing valid values and addresses.
        unsafe {
            libc::select(high_fd,
                         inout_read_fds,
                         null,
                         null,
                         &mut libc_timeout as *mut libc::timeval)
        }
    }
}

struct FileWatcher {
    read_fds: libc::fd_set,
    inout_read_fds: libc::fd_set,
    max_fd: i32,
}

// Interface to the select(2) system call, for ready-to-read only.
impl FileWatcher {
    fn new() -> FileWatcher {
        // The fd sets don't need to be initialized to known values because
        // they are cleared by the following FD_ZERO calls, which are safe
        // because we're passing libc::fd_sets to them.
        let mut read_fds = unsafe { mem::uninitialized::<libc::fd_set>() };
        let mut inout_read_fds = unsafe { mem::uninitialized::<libc::fd_set>() };
        unsafe { libc::FD_ZERO(&mut read_fds); };
        unsafe { libc::FD_ZERO(&mut inout_read_fds); };
        FileWatcher { read_fds, inout_read_fds, max_fd: -1 }
    }

    fn set(&mut self, f: &File) -> Result<()> {
        self.set_fd(f.as_raw_fd())
    }

    // The ..._fd functions exist because other APIs (e.g. dbus) return RawFds
    // instead of Files.
    fn set_fd(&mut self, fd: RawFd) -> Result<()> {
        if fd >= libc::FD_SETSIZE as i32 {
            return Err("set_fd: fd too large".into());
        }
        // see comment for |set()|
        unsafe { libc::FD_SET(fd, &mut self.read_fds); }
        self.max_fd = max(self.max_fd, fd);
        Ok(())
    }

    fn clear_fd(&mut self, fd: RawFd) -> Result<()> {
        if fd >= libc::FD_SETSIZE as i32 {
            return Err("clear_fd: fd is too large".into());
        }
        // see comment for |set()|
        unsafe { libc::FD_CLR(fd, &mut self.read_fds); }
        Ok(())
    }

    // Mut self here and below are required (unnecessarily) by FD_ISSET.
    fn has_fired(&mut self, f: &File) -> Result<bool> {
        self.has_fired_fd(f.as_raw_fd())
    }

    fn has_fired_fd(&mut self, fd: RawFd) -> Result<bool> {
        if fd >= libc::FD_SETSIZE as i32 {
            return Err("has_fired_fd: fd is too large".into());
        }
        // see comment for |set()|
        Ok(unsafe { libc::FD_ISSET(fd, &mut self.inout_read_fds) })
    }

    fn watch(&mut self, timeout: &Duration, timer: &mut Timer) -> Result<usize> {
        self.inout_read_fds = self.read_fds;
        let n = timer.select(self.max_fd + 1, &mut self.inout_read_fds, &timeout);
        match n {
            // into() puts the io::Error in a Box.
            -1 => Err(io::Error::last_os_error().into()),
            n => {
                if n < 0 {
                    Err(format!("unexpected return value {} from select", n).into())
                } else {
                    Ok(n as usize)
                }
            }
        }
    }
}

#[derive(Clone, Copy, Default)]
struct Sample {
    uptime: i64,                // system uptime in ms
    sample_type: SampleType,
    info: Sysinfo,
    runnables: u32,             // number of runnable processes
    available: u32,             // available RAM from low-mem notifier
    vmstat_values: [u32; VMSTAT_VALUES_COUNT],
}

impl Sample {
    // Outputs a sample.
    fn output(&self, out: &mut File) -> Result<()> {
        writeln!(out, "{}.{:02} {:6} {} {} {} {} {} {} {} {} {} {} {} {} {}",
               self.uptime / 1000,
               self.uptime % 1000 / 10,
               self.sample_type,
               self.info.0.loads[0],
               self.info.0.freeram,
               self.info.0.freeswap,
               self.info.0.procs,
               self.runnables,
               self.available,
               self.vmstat_values[0],
               self.vmstat_values[1],
               self.vmstat_values[2],
               self.vmstat_values[3],
               self.vmstat_values[4],
               self.vmstat_values[5],
               self.vmstat_values[6]
        )?;
        Ok(())
    }
}


#[derive(Copy, Clone)]
struct Sysinfo(libc::sysinfo);

impl Sysinfo {
    // Wrapper for sysinfo syscall.
    fn sysinfo() -> Result<Sysinfo> {
        let mut info: Sysinfo = Default::default();
        // sysinfo() is always safe when passed a valid pointer
        match unsafe { libc::sysinfo(&mut info.0 as *mut libc::sysinfo) } {
            0 => Ok(info),
            _ => Err(format!("sysinfo: {}", strerror(errno())).into())
        }
    }

    // Fakes sysinfo system call, for testing.
    fn fake_sysinfo() -> Result<Sysinfo> {
        let mut info: Sysinfo = Default::default();
        // Any numbers will do.
        info.0.loads[0] = 5;
        info.0.freeram = 42_000_000;
        info.0.freeswap = 84_000_000;
        info.0.procs = 1234;
        Ok(info)
    }
}

impl Default for Sysinfo {
    fn default() -> Sysinfo{
        // safe because sysinfo contains only numbers
        unsafe { mem::zeroed() }
    }
}

struct SampleQueue {
    samples: [Sample; SAMPLE_QUEUE_LENGTH],
    head: usize,   // points to latest entry
    count: usize,  // count of valid entries (min=0, max=SAMPLE_QUEUE_LENGTH)
}

impl SampleQueue {
    fn new() -> SampleQueue {
        let s: Sample = Default::default();
        SampleQueue { samples: [s; SAMPLE_QUEUE_LENGTH],
                      head: 0,
                      count: 0,
        }
    }

    // Returns self.head as isize, to make index calculations behave correctly
    // on underflow.
    fn ihead(&self) -> isize {
        self.head as isize
    }

    fn reset(&mut self) {
        self.head = 0;
        self.count = 0;
    }

    // Returns the next slot in the queue.  Always succeeds, since on overflow
    // it discards the LRU slot.
    fn next_slot(&mut self) -> &mut Sample {
        let slot = self.head;
        self.head = modulo(self.ihead() + 1, SAMPLE_QUEUE_LENGTH) as usize;
        if self.count < SAMPLE_QUEUE_LENGTH {
            self.count += 1;
        }
        &mut self.samples[slot]
    }

    fn sample(&self, i: isize) -> &Sample {
        assert!(i >= 0);
        // Subtract 1 because head points to the next free slot.
        assert!(modulo(self.ihead() - 1 - i, SAMPLE_QUEUE_LENGTH) <= self.count,
                "bad queue index: i {}, head {}, count {}", i, self.head, self.count);
        &self.samples[i as usize]
    }

    // Outputs to file |f| samples from |start_time| to the head.  Uses a start
    // time rather than a start index because when we start a clip we have to
    // do a time-based search anyway.
    fn output_from_time(&self, mut f: &mut File, start_time: i64) -> Result<()> {
        // For now just do a linear search. ;)
        let mut start_index = modulo(self.ihead() - 1, SAMPLE_QUEUE_LENGTH);
        debug!("output_from_time: start_time {}, head {}", start_time, start_index);
        while self.samples[start_index].uptime > start_time {
            debug!("output_from_time: seeking uptime {}, index {}",
                   self.samples[start_index].uptime, start_index);
            start_index = modulo(start_index as isize - 1, SAMPLE_QUEUE_LENGTH);
            if start_index == self.head {
                warn!("too many events in requested interval");
                break;
            }
        }

        let mut index = modulo(start_index as isize + 1, SAMPLE_QUEUE_LENGTH) as isize;
        while index != self.ihead() {
            debug!("output_from_time: outputting index {}", index);
            self.sample(index).output(&mut f)?;
            index = modulo(index + 1, SAMPLE_QUEUE_LENGTH) as isize;
        }
        Ok(())
    }
}

// Returns number of tasks in the run queue as reported in /proc/loadavg, which
// must be accessible via runnables_file.
fn get_runnables(runnables_file: &File) -> Result<u32> {
    // It is safe to leave the content of buffer uninitialized because we read
    // into it and only look at the part of it initialized by reading.
    let mut buffer: [u8; PAGE_SIZE] = unsafe { mem::uninitialized() };
    let content = pread(runnables_file, &mut buffer[..])?;
    // Example: "0.81 0.66 0.86 22/3873 7043" (22 runnables here).  The format is
    // fixed, so just skip the first 14 characters.
    if content.len() < 16 {
        return Err("unexpected /proc/loadavg format".into());
    }
    let (value, _) = parse_int_prefix(&content[14..])?;
    Ok(value)
}

// Converts the initial digit characters of |s| to the value of the number they
// represent.  Returns the number of characters scanned.
fn parse_int_prefix(s: &str) -> Result<(u32, usize)> {
    let mut result = 0;
    let mut count = 0;
    // Skip whitespace first.
    for c in s.trim_left().chars() {
        let x = c.to_digit(10);
        match x {
            Some (d) => {
                result = result * 10 + d;
                count += 1;
            },
            None => { break; }
        }
    }
    if count == 0 {
        Err(format!("parse_int_prefix: not an int: {}", s).into())
    } else {
        Ok((result, count))
    }
}

// Writes |string| to file |path|.  If |append| is true, seeks to end first.
// If |append| is false, truncates the file first.
fn write_string(string: &str, path: &Path, append: bool) -> Result<()> {
    let mut f = open_with_flags(&path, OpenOptions::new().write(true).append(append))?;
    if !append {
        f.set_len(0)?;
    }
    f.write_all(string.as_bytes())?;
    Ok(())
}

// Reads selected values from |file| (which should be opened to /proc/vmstat)
// as specified in |VMSTATS|, and stores them in |vmstat_values|.  The format of
// the vmstat file is (<name> <integer-value>\n)+.
fn get_vmstats(file: &File, vmstat_values: &mut [u32]) -> Result<()> {
    // Safe because we read into the buffer, then borrow the part of it that
    // was filled.
    let mut buffer: [u8; PAGE_SIZE] = unsafe { mem::uninitialized() };
    let content = pread(file, &mut buffer[..])?;
    let mut rest = &content[..];
    for (i, &(name, mandatory)) in VMSTATS.iter().enumerate() {
        let found_name = rest.find(name);
        match found_name {
            Some(name_pos) => {
                rest = &rest[name_pos..];
                let found_space = rest.find(' ');
                match found_space {
                    Some(space_pos) => {
                        // Skip name and space.
                        rest = &rest[space_pos+1..];
                        let (value, pos) = parse_int_prefix(rest)?;
                        vmstat_values[i] = value;
                        rest = &rest[pos..];
                    }
                    None => {
                        return Err("unexpected vmstat file syntax".into());
                    }
                }
            }
            None => {
                if mandatory {
                    return Err(format!("missing '{}' from vmstats", name).into());
                } else {
                    vmstat_values[i] = 0;
                }
            }
        }
    }
    Ok(())
}

fn pread_u32(f: &File) -> Result<u32> {
    let mut buffer: [u8; 20] = [0; 20];
    // pread is safe to call with valid addresses and buffer length.
    let length = unsafe {
        libc::pread(f.as_raw_fd(),
                    &mut buffer[0] as *mut u8 as *mut c_void,
                    buffer.len(),
                    0)
    };
    if length < 0 {
        return Err("bad pread_u32".into());
    }
    if length == 0 {
        return Err("empty pread_u32".into());
    }
    Ok(String::from_utf8_lossy(&buffer[..length as usize]).trim().parse::<u32>()?)
}

// Reads the content of file |f| starting at offset 0, up to |PAGE_SIZE|,
// stores it in |read_buffer|, and returns a slice of it as a string.
fn pread<'a>(f: &File, buffer: &'a mut [u8]) -> Result<&'a str> {
    // pread is safe to call with valid addresses and buffer length.
    let length = unsafe {
        libc::pread(f.as_raw_fd(), &mut buffer[0] as *mut u8 as *mut c_void, buffer.len(), 0)
    };
    if length == 0 {
        return Err("unexpected null pread".into());
    }
    if length < 0 {
        return Err(format!("pread failed: {}", strerror(errno())).into());
    }
    // Sysfs files contain only single-byte characters, so from_utf8_unchecked
    // would be safe for those files.  However, there's no guarantee that this
    // is only called on sysfs files.
    Ok(str::from_utf8(&buffer[..length as usize])?)
}

fn read_nonblocking_pipe(file: &mut File, mut buf: &mut [u8]) -> Result<usize> {
    let status = file.read(&mut buf);
    let read_bytes = match status {
        Ok(n) => n,
        Err(_) if errno() == libc::EAGAIN => 0,
        Err(_) => return Err("cannot read pipe".into()),
    };
    Ok(read_bytes)
}


struct Watermarks { min: u32, low: u32, high: u32 }

struct ZoneinfoFile(File);

impl ZoneinfoFile {
    // Computes and returns the watermark values from /proc/zoneinfo.
    fn read_watermarks(&mut self) -> Result<Watermarks> {
        let mut min = 0;
        let mut low = 0;
        let mut high = 0;
        let mut content = String::new();
        self.0.read_to_string(&mut content)?;
        for line in content.lines() {
            let items = line.split_whitespace().collect::<Vec<_>>();
            match items[0] {
                "min" => min += items[1].parse::<u32>()?,
                "low" => low += items[1].parse::<u32>()?,
                "high" => high += items[1].parse::<u32>()?,
                _ => {}
            }
        }
        Ok(Watermarks { min, low, high })
    }
}

trait Dbus {
    // Returns a vector of file descriptors that can be registered with a
    // Watcher.
    fn get_fds(&self) -> &Vec<RawFd>;
    // Processes incoming Chrome dbus events as indicated by |watcher|.
    // Returns counts of tab discards and browser-detected OOMs.
    fn process_chrome_events(&mut self, watcher: &mut FileWatcher) -> Result<(i32,i32)>;
}

struct GenuineDbus {
    connection: dbus::Connection,
    fds: Vec<RawFd>,
}

impl Dbus for GenuineDbus {
    fn get_fds(&self) -> &Vec<RawFd> {
        &self.fds
    }

    // Called if any of the dbus file descriptors fired.  Checks the incoming
    // messages and return counts of relevant messages.
    //
    // For debugging:
    //
    // dbus-monitor --system type=signal,interface=org.chromium.EventLogger
    //
    // dbus-send --system --type=signal / org.chromium.EventLogger.ChromeEvent string:tab-discard
    //
    fn process_chrome_events(&mut self, watcher: &mut FileWatcher) -> Result<(i32, i32)> {
        // Check all delivered messages, and count the messages of interest of
        // each kind.  Then create events for those messages.  Counting first
        // simplifies the borrowing.
        let mut tab_discard_count = 0;
        let mut oom_kill_browser_count = 0;
        {
            for &fd in self.fds.iter() {
                if !watcher.has_fired_fd(fd)? {
                    continue;
                }
                let handle = self.connection.watch_handle(fd, WatchEvent::Readable as libc::c_uint);
                for connection_item in handle {
                    // Only consider signals.
                    if let dbus::ConnectionItem::Signal(ref message) = connection_item {
                        // Only consider signals with "ChromeEvent" members.
                        if let Some(member) = message.member() {
                            if &*member != "ChromeEvent" {
                                continue;
                            }
                        }
                        let items = &message.get_items();
                        if items.is_empty() {
                            continue;
                        }
                        let item = &message.get_items()[0];
                        match item {
                            &dbus::MessageItem::Str(ref s) => match &s[..] {
                                "tab-discard" | "oom-kill" => {
                                    match sample_type_from_signal(s) {
                                        SampleType::TabDiscard => tab_discard_count += 1,
                                        SampleType::OomKillBrowser => oom_kill_browser_count += 1,
                                        x => panic!("unexpected sample type {:?}", x),
                                    }
                                },
                                _ => { warn!("unexpected chrome event type: {}", s); }
                            }
                            _ => warn!("ignoring chrome event with non-string arg1"),
                        }
                    }
                }
            }
        }
        Ok((tab_discard_count, oom_kill_browser_count))
    }
}

impl GenuineDbus {
    // A GenuineDbus object contains a D-Bus connection used to receive
    // information from Chrome about events of interest (e.g. tab discards).
    fn new() -> Result<GenuineDbus> {
        let connection = Connection::get_private(BusType::System)?;
        let _m = connection.add_match(concat!("type='signal',",
                                              "interface='org.chromium.EventLogger',",
                                              "member='ChromeEvent'"));
        let fds = connection.watch_fds().iter().map(|w| w.fd()).collect();
        Ok(GenuineDbus { connection, fds })
    }
}

struct MockDbus {
    fds: Vec<RawFd>,
    fifo_in: File,
    fifo_out: Option<File>,  // using Option merely to use take()
}

impl Dbus for MockDbus {
    fn get_fds(&self) -> &Vec<RawFd> {
        &self.fds
    }

    // Processes any mock chrome events.  Events are strings separated by
    // newlines sent to the event pipe.  We could check if the pipe fired in
    // the watcher, but it's less code to just do a non-blocking read.
    fn process_chrome_events(&mut self, _watcher: &mut FileWatcher) -> Result<(i32, i32)> {
        let mut buf = [0u8; 4096];
        let read_bytes = read_nonblocking_pipe(&mut self.fifo_in, &mut buf)?;
        let events = str::from_utf8(&buf[..read_bytes])?.lines();
        let mut tab_discard_count = 0;
        let mut oom_kill_count = 0;
        for event in events {
            match event {
                "tab-discard" => tab_discard_count += 1,
                "oom-kill" => oom_kill_count += 1,
                other => return Err(format!("unexpected mock event {:?}", other).into()),
            };
        }
        Ok((tab_discard_count, oom_kill_count))
    }
}

impl MockDbus {
    fn new(fifo_path: &Path) -> Result<MockDbus> {
        let fifo_in = OpenOptions::new()
            .custom_flags(libc::O_NONBLOCK)
            .read(true)
            .open(&fifo_path)?;
        let fds = vec![fifo_in.as_raw_fd()];
        let fifo_out = OpenOptions::new()
            .custom_flags(libc::O_NONBLOCK)
            .write(true)
            .open(&fifo_path)?;
        Ok(MockDbus { fds, fifo_in, fifo_out: Some(fifo_out) })
    }
}

// The main object.
struct Sampler<'a> {
    testing: bool,                 // Set to true when running integration test.
    always_poll_fast: bool,        // When true, program stays in fast poll mode.
    paths: &'a Paths,              // Paths of files used by the program.
    dbus: Box<Dbus>,               // Used to receive Chrome event notifications.
    low_mem_margin: u32,           // Low-memory notification margin, assumed to remain constant in
                                   // a boot session.
    sample_header: String,         // The text at the beginning of each clip file.
    files: Files,                  // Files kept open by the program.
    watcher: FileWatcher,          // Maintains a set of files being watched for select().
    low_mem_watcher: FileWatcher,  // A watcher for the low-mem device.
    clip_counter: usize,           // Index of next clip file (also part of file name).
    sample_queue: SampleQueue,     // A running queue of samples of vm quantities.
    current_available: u32,        // Amount of "available" memory (in MB) at last reading.
    current_time: i64,             // Wall clock in ms at last reading.
    collecting: bool,              // True if we're in the middle of collecting a clip.
    timer: Box<Timer>,             // Real or mock timer.
    quit_request: bool,            // Signals a quit-and-restart request when testing.
}

impl<'a> Sampler<'a> {

    fn new(testing: bool,
           always_poll_fast: bool,
           paths: &'a Paths,
           timer: Box<Timer>,
           dbus: Box<Dbus>) -> Sampler {

        let mut low_mem_file_flags = OpenOptions::new();
        low_mem_file_flags.custom_flags(libc::O_NONBLOCK);
        low_mem_file_flags.read(true);
        let low_mem_file_option = open_with_flags_maybe(&paths.low_mem_device, &low_mem_file_flags)
            .expect("error opening low-mem file");
        let mut watcher = FileWatcher::new();
        let mut low_mem_watcher = FileWatcher::new();

        if let Some(ref low_mem_file) = low_mem_file_option.as_ref() {
            watcher.set(&low_mem_file).expect("cannot watch low-mem fd");
            low_mem_watcher.set(&low_mem_file).expect("cannot set low-mem watcher");
        }
        for fd in dbus.get_fds().iter().by_ref() {
            watcher.set_fd(*fd).expect("cannot watch dbus fd");
        }

        let trace_pipe_file = oom_trace_setup(&paths).expect("trace setup error");
        watcher.set(&trace_pipe_file).expect("cannot watch trace_pipe");

        let files = Files {
            vmstat_file: open(&paths.vmstat).expect("cannot open vmstat"),
            runnables_file: open(&paths.runnables).expect("cannot open loadavg"),
            available_file_option: open_maybe(&paths.available)
                .expect("error opening available file"),
            trace_pipe_file,
            low_mem_file_option,
        };

        let sample_header = build_sample_header();

        let mut sampler = Sampler {
            testing,
            always_poll_fast,
            dbus,
            low_mem_margin: read_int(&paths.low_mem_margin).unwrap_or(0),
            paths,
            sample_header,
            files,
            watcher,
            low_mem_watcher,
            sample_queue: SampleQueue::new(),
            clip_counter: 0,
            collecting: false,
            current_available: 0,
            current_time: 0,
            timer,
            quit_request: false,
        };
        sampler.find_starting_clip_counter().expect("cannot find starting clip counter");
        sampler.log_static_parameters().expect("cannot log static parameters");
        sampler
    }

    // Refresh cached time.  This should be called after system calls, which
    // can potentially block, but not if current_time is unused before the next call.
    fn refresh_time(&mut self) {
        self.current_time = self.timer.now();
    }

    // Collect a sample using the latest time snapshot.
    fn enqueue_sample(&mut self, sample_type: SampleType) -> Result<()> {
        let time = self.current_time;  // to pacify the borrow checker
        self.enqueue_sample_at_time(sample_type, time)
    }

    // Collects a sample of memory manager stats and adds it to the sample
    // queue, possibly overwriting an old value.  |sample_type| indicates the
    // type of sample, and |time| the system uptime at the time the sample was
    // collected.
    fn enqueue_sample_at_time(&mut self, sample_type: SampleType, time: i64) -> Result<()> {
        {
            let sample: &mut Sample = self.sample_queue.next_slot();
            sample.uptime = time;
            sample.sample_type = sample_type;
            sample.available = self.current_available;
            sample.runnables = get_runnables(&self.files.runnables_file)?;
            sample.info = if self.testing { Sysinfo::fake_sysinfo()? } else { Sysinfo::sysinfo()? };
            get_vmstats(&self.files.vmstat_file, &mut sample.vmstat_values)?;
        }
        self.refresh_time();
        Ok(())
    }


    // Reads oom-kill events from trace_pipe (passed as |fd|) and adds them to
    // the sample queue.  Returns true if clip collection must be started.
    //
    // TODO(semenzato will open bug): trace pipe can only be used by a single
    // process, so this needs to be demonized.
    fn process_oom_traces(&mut self) -> Result<bool> {
        // It is safe to leave the content of |buffer| uninitialized because we
        // read into it.
        let mut buffer: [u8; PAGE_SIZE] = unsafe { mem::uninitialized() };
        // Safe because we're reading into a valid buffer up to its size.
        let length = unsafe {
            libc::read(self.files.trace_pipe_file.as_raw_fd(),
                       &mut buffer[0] as *mut u8 as *mut c_void, buffer.len())
        };
        if length < 0 {
            return Err("error reading trace pipe".into());
        }
        if length == 0 {
            return Err("unexpected empty trace pipe".into());
        }
        let ul = length as usize;
        // Note: the following assertion is necessary because if the trace output
        // with the oom kill event straddles two buffers, the parser can fail in
        // mysterious ways.
        if ul >= PAGE_SIZE {
            return Err("unexpectedly large input from trace_pipe".into());
        }
        // We trust the kernel to only produce valid utf8 strings.
        let mut rest = unsafe { str::from_utf8_unchecked(&buffer[..ul]) };
        let mut oom_happened = false;

        // Parse output of trace_pipe.  There may be multiple oom kills, and who
        // knows what else.
        loop {
            /* The oom line from trace_pipe looks like this (including the 8
             * initial spaces):
            chrome-13700 [001] .... 867348.061651: oom_kill_process <-out_of_memory
             */
            let oom_kill_function_name = "oom_kill_process";
            if let Some(function_index) = rest.find(oom_kill_function_name) {
                oom_happened = true;
                let slice = &rest[..function_index - 2];
                rest = &rest[function_index + oom_kill_function_name.len() ..];
                // Grab event time.
                let reported_time: f64 = match slice.rfind(' ') {
                    Some(time_index) => {
                        let time_string = &slice[time_index + 1 ..];
                        match time_string.parse() {
                            Ok(value) => value,
                            Err(_) => return Err("cannot parse float from trace pipe".into())
                        }
                    },
                    None => return Err("cannot find space in trace pipe".into())
                };
                // Add two samples, one with the OOM time as reported by the kernel, the
                // other with the current time (firing of the trace event).  Collecting the
                // data twice is a waste, but this is a rare event.
                let time =  (reported_time * 1000.0) as i64;
                self.enqueue_sample_at_time(SampleType::OomKillKernel, time)?;
                self.enqueue_sample(SampleType::OomKillTrace)?;
            } else {
                break;
            }
        }
        Ok(oom_happened)
    }

    // Creates or overwrites a file in the memd log directory containing
    // quantities of interest.
    fn log_static_parameters(&self) -> Result<()> {
        let mut out = &mut OpenOptions::new()
            .write(true)
            .create(true)
            .truncate(true)
            .open(&self.paths.static_parameters)?;
        fprint_datetime(out)?;
        let low_mem_margin = read_int(&self.paths.low_mem_margin).unwrap_or(0);
        writeln!(out, "margin {}", low_mem_margin)?;

        let psv = &self.paths.procsysvm;
        log_from_procfs(&mut out, psv, "min_filelist_kbytes")?;
        log_from_procfs(&mut out, psv, "min_free_kbytes")?;
        log_from_procfs(&mut out, psv, "extra_free_kbytes")?;

        let mut zoneinfo = ZoneinfoFile { 0: open(&self.paths.zoneinfo)? };
        let watermarks = zoneinfo.read_watermarks()?;
        writeln!(out, "min_water_mark_kbytes {}", watermarks.min * 4)?;
        writeln!(out, "low_water_mark_kbytes {}", watermarks.low * 4)?;
        writeln!(out, "high_water_mark_kbytes {}", watermarks.high * 4)?;
        Ok(())
    }

    // Returns true if the program should go back to slow-polling mode (or stay
    // in that mode).  Returns false otherwise.  Relies on |self.collecting|
    // and |self.current_available| being up-to-date.
    fn should_poll_slowly(&self) -> bool {
        !self.collecting &&
            !self.always_poll_fast &&
            self.current_available > LOW_MEM_SAFETY_FACTOR * self.low_mem_margin
    }

    // Sits mostly idle and checks available RAM at low frequency.  Returns
    // when available memory gets "close enough" to the tab discard margin.
    fn slow_poll(&mut self) -> Result<()> {
        debug!("entering slow poll at {}", self.current_time);
        // Idiom for do ... while.
        while {
            self.timer.sleep(&SLOW_POLL_PERIOD_DURATION);
            self.quit_request = self.timer.quit_request();
            if let Some(ref available_file) = self.files.available_file_option.as_ref() {
                self.current_available = pread_u32(available_file)?;
            }
            self.refresh_time();
            self.should_poll_slowly() && !self.quit_request
        } {}
        Ok(())
    }

    // Collects timer samples at fast rate.  Also collects event samples.
    // Samples contain various system stats related to kernel memory
    // management.  The samples are placed in a circular buffer.  When
    // something "interesting" happens, (for instance a tab discard, or a
    // kernel OOM-kill) the samples around that event are saved into a "clip
    // file".
    fn fast_poll(&mut self) -> Result<()> {
        let mut earliest_start_time = self.current_time;
        debug!("entering fast poll at {}", earliest_start_time);

        // Collect the first timer sample immediately upon entering.
        self.enqueue_sample(SampleType::Timer)?;
        // Keep track if we're in a low-mem state.  Initially assume we are
        // not.
        let mut in_low_mem = false;
        // |clip_{start,end}_time| are the collection start and end time for
        // the current clip.  Their value is valid only when |self.collecting|
        // is true.
        let mut clip_start_time = self.current_time;
        let mut clip_end_time = self.current_time;
        // |final_collection_time| indicates when we should stop collecting
        // samples for any clip (either the current one, or the next one).  Its
        // value is valid only when |self.collecting| is true.
        let mut final_collection_time = self.current_time;
        let null_duration = Duration::from_secs(0);

        // |self.collecting| is true when we're in the middle of collecting a clip
        // because something interesting has happened.
        self.collecting = false;

        // Poll/select loop.
        loop {
            // Assume event is not interesting (since most events
            // aren't). Change this to true for some event types.
            let mut event_is_interesting = false;

            let watch_start_time = self.current_time;
            let fired_count = {
                let mut watcher = &mut self.watcher;
                watcher.watch(&FAST_POLL_PERIOD_DURATION, &mut *self.timer)?
            };
            self.quit_request = self.timer.quit_request();
            if let Some(ref available_file) = self.files.available_file_option.as_ref() {
                self.current_available = pread_u32(available_file)?;
            }
            self.refresh_time();

            // Record a sample when we sleep too long.  Such samples are
            // somewhat redundant as they could be deduced from the log, but we
            // wish to make it easier to detect such (rare, we hope)
            // occurrences.
            if watch_start_time > self.current_time + UNREASONABLY_LONG_SLEEP {
                warn!("woke up at {} after unreasonable {} sleep",
                      self.current_time, self.current_time - watch_start_time);
                self.enqueue_sample(SampleType::Sleeper)?;
            }

            if fired_count == 0 {
                // Timer event.
                self.enqueue_sample(SampleType::Timer)?;
            } else {
                // The low_mem file is level-triggered, not edge-triggered (my bad)
                // so we don't watch it when memory stays low, because it would
                // fire continuously.  Instead we poll it at every event, and when
                // we detect a low-to-high transition we start watching it again.
                // Unfortunately this requires an extra select() call: however we
                // don't expect to spend too much time in this state (and when we
                // do, this is the least of our worries).
                let low_mem_has_fired = match self.files.low_mem_file_option {
                    Some(ref low_mem_file) => self.watcher.has_fired(low_mem_file)?,
                    _ => false,
                };
                if low_mem_has_fired {
                    debug!("entering low mem at {}", self.current_time);
                    in_low_mem = true;
                    self.enqueue_sample(SampleType::EnterLowMem)?;
                    let fd = self.files.low_mem_file_option.as_ref().unwrap().as_raw_fd();
                    self.watcher.clear_fd(fd)?;
                    // Make this interesting at least until chrome events are
                    // plumbed, maybe permanently.
                    event_is_interesting = true;
                } else if in_low_mem && self.low_mem_watcher.watch(&null_duration,
                                                                   &mut *self.timer)? == 0 {
                    // Refresh time since we may have blocked.  (That should
                    // not happen often because currently the run times between
                    // sleeps are well below the minimum timeslice.)
                    self.current_time = self.timer.now();
                    debug!("leaving low mem at {}", self.current_time);
                    in_low_mem = false;
                    self.enqueue_sample(SampleType::LeaveLowMem)?;
                    self.watcher.set(&self.files.low_mem_file_option.as_ref().unwrap())?;
                }

                // Check for Chrome events.
                let (tab_discard_count, oom_kill_browser_count) =
                    self.dbus.process_chrome_events(&mut self.watcher)?;
                for _ in 0..tab_discard_count {
                    self.enqueue_sample(SampleType::TabDiscard)?;
                }
                for _ in 0..oom_kill_browser_count {
                    self.enqueue_sample(SampleType::OomKillBrowser)?;
                }
                if tab_discard_count + oom_kill_browser_count > 0 {
                    debug!("chrome events at {}", self.current_time);
                    event_is_interesting = true;
                }

                // Check for OOM-kill event.
                if self.watcher.has_fired(&self.files.trace_pipe_file)? {
                    if self.process_oom_traces()? {
                        debug!("oom trace at {}", self.current_time);
                        event_is_interesting = true;
                    }
                }
            }

            // Arrange future saving of samples around interesting events.
            if event_is_interesting {
                // Update the time intervals to ensure we include all samples
                // of interest in a clip.  If we're not in the middle of
                // collecting a clip, start one.  If we're in the middle of
                // collecting a clip which can be extended, do that.
                final_collection_time = self.current_time + COLLECTION_DELAY_MS;
                if self.collecting {
                    // Check if the current clip can be extended.
                    if clip_end_time < clip_start_time + CLIP_COLLECTION_SPAN_MS {
                        clip_end_time = final_collection_time.min(
                            clip_start_time + CLIP_COLLECTION_SPAN_MS);
                        debug!("extending clip to {}", clip_end_time);
                    }
                } else {
                    // Start the clip collection.
                    self.collecting = true;
                    clip_start_time = earliest_start_time
                        .max(self.current_time - COLLECTION_DELAY_MS);
                    clip_end_time = self.current_time + COLLECTION_DELAY_MS;
                    debug!("starting new clip from {} to {}", clip_start_time, clip_end_time);
                }
            }

            // Check if it is time to save the samples into a file.
            if self.collecting && self.current_time > clip_end_time - SAMPLING_PERIOD_MS {
                // Save the clip to disk.
                debug!("[{}] saving clip: ({} ... {}), final {}",
                       self.current_time, clip_start_time, clip_end_time, final_collection_time);
                self.save_clip(clip_start_time)?;
                self.collecting = false;
                earliest_start_time = clip_end_time;
                // Need to schedule another collection?
                if final_collection_time > clip_end_time {
                    // Continue collecting by starting a new clip.  Note that
                    // the clip length may be less than CLIP_COLLECTION_SPAN.
                    // This happens when event spans overlap, and also if we
                    // started fast sample collection just recently.
                    assert!(final_collection_time <= clip_end_time + CLIP_COLLECTION_SPAN_MS);
                    clip_start_time = clip_end_time;
                    clip_end_time = final_collection_time;
                    self.collecting = true;
                    debug!("continue collecting with new clip ({} {})",
                           clip_start_time, clip_end_time);
                    // If we got stuck in the select() for a very long time
                    // because of system slowdown, it may be time to collect
                    // this second clip as well.  But we don't bother, since
                    // this is unlikely, and we can collect next time around.
                    if self.current_time > clip_end_time {
                        debug!("heavy slowdown: postponing collection of ({}, {})",
                               clip_start_time, clip_end_time);
                    }
                }
            }
            if self.should_poll_slowly() || (self.quit_request && !self.collecting) {
                break;
            }
        }
        Ok(())
    }

    // Returns the clip file pathname to be used after the current one,
    // and advances the clip counter.
    fn next_clip_path(&mut self) -> PathBuf {
        let name = format!("memd.clip{:03}.log", self.clip_counter);
        self.clip_counter = modulo(self.clip_counter as isize + 1, MAX_CLIP_COUNT);
        self.paths.log_directory.join(name)
    }

    // Finds and sets the starting value for the clip counter in this session.
    // The goal is to preserve the most recent clips (if any) from previous
    // sessions.
    fn find_starting_clip_counter(&mut self) -> Result<()> {
        self.clip_counter = 0;
        let mut previous_time = std::time::UNIX_EPOCH;
        loop {
            let path = self.next_clip_path();
            if !path.exists() {
                break;
            }
            let md = std::fs::metadata(path)?;
            let time = md.modified()?;
            if time < previous_time {
                break;
            } else {
                previous_time = time;
            }
        }
        // We found the starting point, but the counter has already advanced so we
        // need to backtrack one step.
        self.clip_counter = modulo(self.clip_counter as isize - 1, MAX_CLIP_COUNT);
        Ok(())
    }

    // Stores samples of interest in a new clip log, and remove those samples
    // from the queue.
    fn save_clip(&mut self, start_time: i64) -> Result<()> {
        let path = self.next_clip_path();
        let mut out = &mut OpenOptions::new().write(true).create(true).truncate(true).open(path)?;

        // Print courtesy header.  The first line is the current time.  The
        // second line lists the names of the variables in the following lines,
        // in the same order.
        fprint_datetime(out)?;
        out.write_all(self.sample_header.as_bytes())?;
        // Output samples from |start_time| to the head.
        self.sample_queue.output_from_time(&mut out, start_time)?;
        // The queue is now empty.
        self.sample_queue.reset();
        Ok(())
    }
}

// Sets up tracing system to report OOM-kills.  Returns file to monitor for
// OOM-kill events.
//
// TODO(crbug.com/840574): change this to get events from tracing daemon
// instead (probably via D-Bus).
fn oom_trace_setup(paths: &Paths) -> Result<File> {
    write_string("function", &paths.current_tracer, false)?;
    write_string("oom_kill_process", &paths.set_ftrace_filter, true)?;
    let tracing_enabled_file = open(&paths.tracing_enabled)?;
    if pread_u32(&tracing_enabled_file)? != 1 {
        return Err("tracing is disabled".into());
    }
    let tracing_on_file = open(&paths.tracing_on)?;
    if pread_u32(&tracing_on_file)? != 1 {
        return Err("tracing is off".into());
    }
    Ok(OpenOptions::new()
       .custom_flags(libc::O_NONBLOCK)
       .read(true)
       .open(&paths.trace_pipe)?)
}

// Prints |name| and value of entry /pros/sys/vm/|name| (or 0, if the entry is
// missing) to file |out|.
fn log_from_procfs(out: &mut File, dir: &Path, name: &str) -> Result<()> {
    let procfs_path = dir.join(name);
    let value = read_int(&procfs_path).unwrap_or(0);
    writeln!(out, "{} {}", name, value)?;
    Ok(())
}

// Outputs readable date and time to file |out|.
fn fprint_datetime(out: &mut File) -> Result<()> {
    let local: DateTime<Local> = Local::now();
    writeln!(out, "{}", local)?;
    Ok(())
}

#[derive(Copy, Clone, Debug)]
enum SampleType {
    EnterLowMem,        // Entering low-memory state, from the kernel low-mem notifier.
    LeaveLowMem,        // Leaving low-memory state, from the kernel low-mem notifier.
    OomKillBrowser,     // Chrome browser letting us know it detected OOM kill.
    OomKillKernel,      // OOM kill notification from kernel (using kernel time stamp).
    OomKillTrace,       // OOM kill notification from kernel (using current time).
    Sleeper,            // Memd was not running for a long time.
    TabDiscard,         // Chrome browser letting us know about a tab discard.
    Timer,              // Event was generated after FAST_POLL_PERIOD_DURATION with no other events.
    Uninitialized,      // Internal use.
    Unknown,            // Unexpected D-Bus signal.
}

impl Default for SampleType {
    fn default() -> SampleType {
        SampleType::Uninitialized
    }
}

impl fmt::Display for SampleType {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.name())
    }
}

impl SampleType {
    // Returns the 6-character(max) identifier for a sample type.
    fn name(&self) -> &'static str {
        match *self {
            SampleType::EnterLowMem => "lowmem",
            SampleType::LeaveLowMem => "lealow",
            SampleType::OomKillBrowser => "oomkll",  // OOM from chrome
            SampleType::OomKillKernel => "keroom",   // OOM from kernel, with kernel time stamp
            SampleType::OomKillTrace => "traoom",    // OOM from kernel, with current time stamp
            SampleType::Sleeper => "sleepr",
            SampleType::TabDiscard => "discrd",
            SampleType::Timer => "timer",
            SampleType::Uninitialized => "UNINIT",
            SampleType::Unknown => "UNKNWN",
        }
    }
}

// Returns the sample type for a Chrome signal (the string payload in the DBus
// message).
fn sample_type_from_signal(signal: &str) -> SampleType {
    match signal {
        "oom-kill" => SampleType::OomKillBrowser,
        "tab-discard" => SampleType::TabDiscard,
        _ => SampleType::Unknown,
    }
}

// Path names of various system files, mostly in /proc, /sys, and /dev.  They
// are collected into this struct because they get special values when testing.
#[derive(Clone)]
struct Paths {
    vmstat: PathBuf,
    available: PathBuf,
    runnables: PathBuf,
    low_mem_margin: PathBuf,
    low_mem_device: PathBuf,
    current_tracer: PathBuf,
    set_ftrace_filter: PathBuf,
    tracing_enabled: PathBuf,
    tracing_on: PathBuf,
    trace_pipe: PathBuf,
    log_directory: PathBuf,
    static_parameters: PathBuf,
    zoneinfo: PathBuf,
    procsysvm: PathBuf,
    mock_dbus_fifo: PathBuf,
    testing_root: PathBuf,
}

// Returns a file name that replaces |name| when testing.
fn test_filename(testing: bool, testing_root: &str, name: &str) -> String {
    if testing {
        testing_root.to_string() + name
    } else {
        name.to_owned()
    }
}

// This macro constructs a "normal" Paths object when |testing| is false, and
// one that mimics a root filesystem in a local directory when |testing| is
// true.
macro_rules! make_paths {
    ($testing:expr, $root: expr,
     $($name:ident : $e:expr,)*
    ) => (
        Paths {
            testing_root: PathBuf::from($root),
            $($name: PathBuf::from(test_filename($testing, $root, &($e).to_string()))),*
        }
    )
}

// All files that are to be left open go here.  We keep them open to reduce the
// number of syscalls.  They are mostly files in /proc and /sys.
struct Files {
    vmstat_file: File,
    runnables_file: File,
    trace_pipe_file: File,
    // These files might not exist.
    available_file_option: Option<File>,
    low_mem_file_option: Option<File>,
}

fn build_sample_header() -> String {
    let mut s = "uptime type load freeram freeswap procs runnables available".to_string();
    for vmstat in VMSTATS.iter() {
        s = s + " " + vmstat.0;
    }
    s + "\n"
}

fn main() {
    let mut testing = false;
    let mut always_poll_fast = false;

    debug!("memd started");

    let args: Vec<String> = std::env::args().collect();
    for arg in &args[1..] {
        match arg.as_ref() {
            "test" => testing = true,
            "always-poll-fast" => always_poll_fast = true,
            _ => panic!("usage: memd [test|always-poll-fast]*")
        }
    }

    // Send log messages to stdout in test mode, to syslog otherwise.
    if testing {
        env_logger::init();
    } else {
        syslog::init(syslog::Facility::LOG_USER,
                     log::LevelFilter::Warn,
                     Some("memd")).expect("cannot initialize syslog");
    }

    run_memory_daemon(testing, always_poll_fast);
}

#[test]
fn memory_daemon_test() {
    run_memory_daemon(true, false);
}

fn testing_root() -> String {
    // Calling getpid() is always safe.
    format!("/tmp/memd-testing-root-{}", unsafe { libc::getpid() })
}

fn run_memory_daemon(testing: bool, always_poll_fast: bool) {
    warn!("memd started");
    let testing_root = testing_root();
    // make_paths! returns a Paths object initializer with these fields.
    let paths = make_paths!(
        testing,
        &testing_root,
        vmstat:            "/proc/vmstat",
        available:         LOW_MEM_SYSFS.to_string() + "/available",
        runnables:         "/proc/loadavg",
        low_mem_margin:    LOW_MEM_SYSFS.to_string() + "/margin",
        low_mem_device:    LOW_MEM_DEVICE,
        current_tracer:    TRACING_SYSFS.to_string() + "/current_tracer",
        set_ftrace_filter: TRACING_SYSFS.to_string() + "/set_ftrace_filter",
        tracing_enabled:   TRACING_SYSFS.to_string() + "/tracing_enabled",
        tracing_on:        TRACING_SYSFS.to_string() + "/tracing_on",
        trace_pipe:        TRACING_SYSFS.to_string() + "/trace_pipe",
        log_directory:     LOG_DIRECTORY,
        static_parameters: LOG_DIRECTORY.to_string() + "/" + STATIC_PARAMETERS_LOG,
        zoneinfo:          "/proc/zoneinfo",
        procsysvm:         "/proc/sys/vm",
        mock_dbus_fifo:    "/".to_string() + TESTING_MOCK_DBUS_FIFO_NAME,
    );

    if testing {
        setup_test_environment(&paths);
        let var_log = &paths.log_directory.parent().unwrap();
        std::fs::create_dir_all(var_log).expect("cannot create /var/log");
    }

    // Make sure /var/log/memd exists.  Create it if not.  Assume /var/log
    // exists.  Panic on errors.
    if !paths.log_directory.exists() {
        create_dir(&paths.log_directory)
            .expect(&format!("cannot create log directory {:?}", &paths.log_directory));
    }

    if !testing {
        let timer = Box::new(GenuineTimer {});
        let dbus = Box::new(GenuineDbus::new().expect("cannot connect to dbus"));
        let mut sampler = Sampler::new(testing, always_poll_fast, &paths, timer, dbus);
        loop {
            // Run forever, alternating between slow and fast poll.
            sampler.slow_poll().expect("slow poll error");
            sampler.fast_poll().expect("fast poll error");
        }
    } else {
        for test_desc in TEST_DESCRIPTORS.iter() {
            // Every test run requires a (mock) restart of the daemon.
            println!("--------------\nrunning test:\n{}", test_desc);
            // Clean up log directory.
            std::fs::remove_dir_all(&paths.log_directory).expect("cannot remove /var/log/memd");
            std::fs::create_dir_all(&paths.log_directory).expect("cannot create /var/log/memd");

            let events = events_from_test_descriptor(test_desc);
            let mut dbus = Box::new(MockDbus::new(&paths.mock_dbus_fifo)
                                    .expect("cannot create mock dbus"));
            let timer = Box::new(MockTimer::new(events, paths.clone(),
                                                dbus.fifo_out.take().unwrap()));
            let mut sampler = Sampler::new(testing, always_poll_fast, &paths, timer, dbus);
            loop {
                // Alternate between slow and fast poll.
                sampler.slow_poll().expect("slow poll error");
                if sampler.quit_request {
                    break;
                }
                sampler.fast_poll().expect("fast poll error");
                if sampler.quit_request {
                    break;
                }
            }
            verify_test_results(test_desc, &paths.log_directory)
                .expect(&format!("test:{}failed.", test_desc));
            println!("test succeeded\n--------------");
        }
        teardown_test_environment(&paths);
    }
}

// ================
// Test Descriptors
// ================
//
// Define events and expected result using "ASCII graphics".
//
// The top lines of the test descriptor (all lines except the last one) define
// sequences of events.  The last line describes the expected result.
//
// Events are single characters:
//
// M = start medium pressure (fast poll)
// H = start high pressure (low-mem notification)
// L = start low pressure (slow poll)
// <digit> = tab discard
// K = kernel OOM kill
// k = chrome notification of OOM kill
// ' ', . = nop (just wait 1 second)
// | = ignored (no delay), cosmetic only
//
// - each character indicates a 1-second slot
// - events (if any) happen at the beginning of their slot
// - multiple events in the same slot are stacked vertically
//
// Example:
//
// ..H.1..L
//     2
//
// means:
//  - wait 2 seconds
//  - signal high-memory pressure, wait 1 second
//  - wait 1 second
//  - signal two tab discard events (named 1 and 2), wait 1 second
//  - wait 2 more seconds
//  - return to low-memory pressure
//
// The last line describes the expected clip logs.  Each log is identified by
// one digit: 0 for memd.clip000.log, 1 for memd.clip001.log etc.  The positions
// of the digits correspond to the time span covered by each clip.  So a clip
// file whose description is 5 characters long is supposed to contain 5 seconds
// worth of samples.
//
// For readability, the descriptor must start and end with newlines, which are
// removed.  Also, indentation (common all-space prefixes) is removed.

const TEST_DESCRIPTORS: &[&str] = &[
    // Very simple test: go from slow poll to fast poll and back.  No clips
    // are collected.
    "
    .M.L.
    .....
    ",

    // Simple test: start fast poll, signal low-mem, signal tab discard.
    "
    .M...H..1.....L
    ..00000000001..
    ",
    // Two full disjoint clips.  Also tests kernel-reported and chrome-reported OOM
    // kills.
    "
    .M......K.............k.....
    ...0000000000....1111111111.
    ",

    // Test that clip collection continues for the time span of interest even if
    // memory pressure returns quickly to a low level.  Note that the
    // medium-pressure event (M) is at t = 1s, but the fast poll starts at 2s
    // (multiple of 2s slow-poll period).

    "
    .MH1L.....
    ..000000..
    ",

    // Several discards, which result in three consecutive clips.  Tab discards 1
    // and 2 produce an 8-second clip because the first two seconds of data are
    // missing.  Also see the note above regarding fast poll start.
    "
    ...M.H12..|...3...6..|.7.....L
              |   4      |
              |   5      |
    ....000000|0011111111|112222..
    ",
];

fn trim_descriptor(descriptor: &str) -> Vec<Vec<u8>> {
    // Remove vertical bars.  Don't check for consistent use because it's easy
    // enough to notice visually.
    let barless_descriptor: String = descriptor.chars().filter(|c| *c != '|').collect();
    // Split string into lines.
    let all_lines: Vec<String> = barless_descriptor.split('\n').map(String::from).collect();
    // A test descriptor must start and end with empty lines, and have at least
    // one line of events, and exactly one line to describe the clip files.
    assert!(all_lines.len() >= 4, "invalid test descriptor format");
    // Remove first and last line.
    let valid_lines = all_lines[1 .. all_lines.len()-1].to_vec();
    // Find indentation amount.  Unwrap() cannot fail because of previous assert.
    let indent = valid_lines.iter().map(|s| s.len() - s.trim_left().len()).min().unwrap();
    // Remove indentation.
    let trimmed_lines: Vec<Vec<u8>> =
        valid_lines.iter().map(|s| s[indent..].to_string().into_bytes()).collect();
    trimmed_lines
}

fn events_from_test_descriptor(descriptor: &str) -> Vec<TestEvent> {
    let all_descriptors = trim_descriptor(descriptor);
    let event_sequences = &all_descriptors[..all_descriptors.len() - 1];
    let max_length = event_sequences.iter().map(|d| d.len()).max().unwrap();
    let mut events = vec![];
    for i in 0 .. max_length {
        for seq in event_sequences {
            // Each character represents one second.  Time unit is milliseconds.
            let mut opt_type = None;
            if i < seq.len() {
                match seq[i] {
                    b'0' | b'1' | b'2' | b'3' | b'4' |
                    b'5' | b'6' | b'7' | b'8' | b'9'
                        => opt_type = Some(TestEventType::TabDiscard),
                    b'H' => opt_type = Some(TestEventType::EnterHighPressure),
                    b'M' => opt_type = Some(TestEventType::EnterMediumPressure),
                    b'L' => opt_type = Some(TestEventType::EnterLowPressure),
                    b'k' => opt_type = Some(TestEventType::OomKillBrowser),
                    b'K' => opt_type = Some(TestEventType::OomKillKernel),
                    b'.' | b' ' | b'|' => {},
                    x => panic!("unexpected character {} in descriptor '{}'", &x, descriptor),
                }
            }
            if let Some(t) = opt_type {
                events.push(TestEvent { time: i as i64 * 1000, event_type: t });
            }
        }
    }
    events
}

// Given a descriptor string for the expected clips, returns a vector of start
// and end time of each clip.
fn expected_clips(descriptor: &[u8]) -> Vec<(i64, i64)> {
    let mut time = 0;
    let mut clip_start_time = 0;
    let mut previous_clip = b'0' - 1;
    let mut previous_char = 0u8;
    let mut clips = vec![];

    for &c in descriptor {
        if c != previous_char {
            if (previous_char as char).is_digit(10) {
                // End of clip.
                clips.push((clip_start_time, time));
            }
            if (c as char).is_digit(10) {
                // Start of clip.
                clip_start_time = time;
                assert_eq!(c, previous_clip + 1, "malformed clip descriptor");
                previous_clip = c;
            }
        }
        previous_char = c;
        time += 1000;
    }
    clips
}

// Converts a string starting with a timestamp in seconds (#####.##, with two
// decimal digits) to a timestamp in milliseconds.
fn time_from_sample_string(line: &str) -> Result<i64> {
    let mut tokens = line.split(|c: char| !c.is_digit(10));
    let seconds = match tokens.next() {
        Some(digits) => digits.parse::<i64>().unwrap(),
        None => return Err("no digits in string".into())
    };
    let centiseconds = match tokens.next() {
        Some(digits) => if digits.len() == 2 {
            digits.parse::<i64>().unwrap()
        } else {
            return Err("expecting 2 decimals".into());
        },
        None => return Err("expecting at least two groups of digits".into()),
    };
    Ok(seconds * 1000 + centiseconds * 10)
}

macro_rules! assert_approx_eq {
    ($x:expr, $y: expr, $tolerance: expr, $format:expr $(, $arg:expr)*) => {{
        let x = $x;
        let y = $y;
        let tolerance = $tolerance;
        let y_min = y - tolerance;
        let y_max = y + tolerance;
        assert!(x < y_max && x > y_min, $format $(, $arg)*);
    }}
}

fn check_clip(clip_times: (i64, i64), clip_path: PathBuf, events: &Vec<TestEvent>) -> Result<()> {
    let clip_name = clip_path.to_string_lossy();
    let mut clip_file = File::open(&clip_path)?;
    let mut file_content = String::new();
    clip_file.read_to_string(&mut file_content)?;
    let lines = file_content.lines().collect::<Vec<&str>>();
    // First line is time stamp.  Second line is field names.  Check count of
    // field names and field values in the third line (don't bother to check
    // the other lines).
    let name_count = lines[1].split_whitespace().count();
    let value_count = lines[2].split_whitespace().count();
    assert_eq!(name_count, value_count);

    // Check first and last time stamps.
    let start_time = time_from_sample_string(&lines[2]).expect("cannot parse first timestamp");
    let end_time = time_from_sample_string(&lines[lines.len() - 1])
        .expect("cannot parse last timestamp");
    let expected_start_time = clip_times.0;
    let expected_end_time = clip_times.1;
    // Milliseconds of slack allowed on start/stop times.  We allow one full
    // fast poll period to take care of edge cases.  The specs don't need to be
    // tight here because it doesn't matter if we collect one fewer sample (or
    // an extra one) at each end.
    let slack_ms = 101i64;
    assert_approx_eq!(start_time, expected_start_time, slack_ms,
                      "unexpected start time for {}", clip_name);
    assert_approx_eq!(end_time, expected_end_time, slack_ms,
                      "unexpected end time for {}", clip_name);

    // Check sample count.
    let expected_sample_count_from_events: usize = events.iter()
        .map(|e| if e.time <= start_time || e.time > end_time {
            0
        } else {
            match e.event_type {
                // OomKillKernel generates two samples.
                TestEventType::OomKillKernel => 2,
                // These generate 0 samples.
                TestEventType::EnterLowPressure |
                TestEventType::EnterMediumPressure => 0,
                _ => 1,
            }
        })
        .sum();
    // We include samples both at the beginning and end of the range, so we
    // need to add 1.  Note that here we use the actual sample times, not the
    // expected times.
    let expected_sample_count_from_timer = ((end_time - start_time) / 100) as usize + 1;
    let expected_sample_count = expected_sample_count_from_events + expected_sample_count_from_timer;
    let sample_count = lines.len() - 2;
    assert_eq!(sample_count, expected_sample_count, "unexpected sample count for {}", clip_name);
    Ok(())
}

fn verify_test_results(descriptor: &str, log_directory: &PathBuf) -> Result<()> {
    let all_descriptors = trim_descriptor(descriptor);
    let result_descriptor = &all_descriptors[all_descriptors.len() - 1];
    let clips = expected_clips(result_descriptor);
    let events = events_from_test_descriptor(descriptor);

    // Check that there are no more clips than expected.
    let files_count = std::fs::read_dir(log_directory)?.count();
    assert_eq!(clips.len() + 1, files_count, "wrong number of clip files");

    let mut clip_number = 0;
    for clip in clips {
        let clip_path = log_directory.join(format!("memd.clip{:03}.log", clip_number));
        check_clip(clip, clip_path, &events)?;
        clip_number += 1;
    }
    Ok(())
}

fn create_dir_all(path: &Path) -> Result<()> {
    let result = std::fs::create_dir_all(path);
    match result {
        Ok(_) => Ok(()),
        Err(e) => Err(format!("create_dir_all: {}: {:?}", path.to_string_lossy(), e).into())
    }
}

fn teardown_test_environment (paths: &Paths) {
    if let Err(e) = std::fs::remove_dir_all(&paths.testing_root) {
        info!("teardown: could not remove {}: {:?}", paths.testing_root.to_str().unwrap(), e);
    }
}

fn setup_test_environment(paths: &Paths) {
    std::fs::create_dir(&paths.testing_root).
        expect(&format!("cannot create {}", paths.testing_root.to_str().unwrap()));
    mkfifo(&paths.mock_dbus_fifo).expect("failed to make mock dbus fifo");
    create_dir_all(paths.vmstat.parent().unwrap()).expect("cannot create /proc");
    create_dir_all(paths.available.parent().unwrap()).expect("cannot create ../chromeos-low-mem");
    create_dir_all(paths.trace_pipe.parent().unwrap()).expect("cannot create ../tracing");
    let sys_vm = paths.testing_root.join("proc/sys/vm");
    create_dir_all(&sys_vm).expect("cannot create /proc/sys/vm");
    create_dir_all(paths.low_mem_device.parent().unwrap()).expect("cannot create /dev");

    let vmstat_content = "\
nr_free_pages 50489
nr_alloc_batch 3326
nr_inactive_anon 48591
nr_active_anon 145673
nr_inactive_file 49597
nr_active_file 50199
nr_unevictable 0
nr_mlock 0
nr_anon_pages 180171
nr_mapped 63194
nr_file_pages 149074
nr_dirty 136
nr_writeback 0
nr_slab_reclaimable 10484
nr_slab_unreclaimable 16134
nr_page_table_pages 9255
nr_kernel_stack 979
nr_unstable 0
nr_bounce 0
nr_vmscan_write 60544150
nr_vmscan_immediate_reclaim 29587
nr_writeback_temp 0
nr_isolated_anon 0
nr_isolated_file 0
nr_shmem 8982
nr_dirtied 3909911
nr_written 64426230
nr_pages_scanned 88
workingset_refault 16082484
workingset_activate 1186339
workingset_nodereclaim 0
nr_anon_transparent_hugepages 2
nr_free_cma 0
nr_dirty_threshold 68740
nr_dirty_background_threshold 5728
pgpgin 41070493
pgpgout 16914684
pswpin 59158894
pswpout 60513438
pgalloc_dma 3247
pgalloc_dma32 131210265
pgalloc_normal 0
pgalloc_movable 0
pgfree 140534607
pgactivate 70428610
pgdeactivate 79401090
pgfault 110047495
pgmajfault 52561387
pgmajfault_s 97569
pgmajfault_a 52314090
pgmajfault_f 149728
pgrefill_dma 4749
pgrefill_dma32 79973171
pgrefill_normal 0
pgrefill_movable 0
pgsteal_kswapd_dma 112
pgsteal_kswapd_dma32 62450553
pgsteal_kswapd_normal 0
pgsteal_kswapd_movable 0
pgsteal_direct_dma 0
pgsteal_direct_dma32 18534641
pgsteal_direct_normal 0
pgsteal_direct_movable 0
pgscan_kswapd_dma 4776
pgscan_kswapd_dma32 126413953
pgscan_kswapd_normal 0
pgscan_kswapd_movable 0
pgscan_direct_dma 11
pgscan_direct_dma32 36676518
pgscan_direct_normal 0
pgscan_direct_movable 0
pgscan_direct_throttle 0
pginodesteal 3302
slabs_scanned 24535326
kswapd_inodesteal 237022
kswapd_low_wmark_hit_quickly 24998
kswapd_high_wmark_hit_quickly 40300
pageoutrun 87752
allocstall 376923
pgrotated 247987
drop_pagecache 0
drop_slab 0
pgmigrate_success 8814551
pgmigrate_fail 94348
compact_migrate_scanned 86192323
compact_free_scanned 1982032044
compact_isolated 19084449
compact_stall 20280
compact_fail 17323
compact_success 2957
unevictable_pgs_culled 0
unevictable_pgs_scanned 0
unevictable_pgs_rescued 0
unevictable_pgs_mlocked 0
unevictable_pgs_munlocked 0
unevictable_pgs_cleared 0
unevictable_pgs_stranded 0
thp_fault_alloc 15
thp_fault_fallback 0
thp_collapse_alloc 7894
thp_collapse_alloc_failed 4784
thp_split 13
thp_zero_page_alloc 0
thp_zero_page_alloc_failed 0
";
    let zoneinfo_content = "\
Node 0, zone      DMA
  pages free     1756
        min      179
        low      223
        high     268
        scanned  0
        spanned  4095
        present  3999
        managed  3977
    nr_free_pages 1756
    nr_alloc_batch 45
    nr_inactive_anon 0
    nr_active_anon 0
    nr_inactive_file 238
    nr_active_file 377
    nr_unevictable 0
    nr_mlock     0
    nr_anon_pages 0
    nr_mapped    284
    nr_file_pages 615
    nr_dirty     0
    nr_writeback 0
    nr_slab_reclaimable 23
    nr_slab_unreclaimable 22
    nr_page_table_pages 2
    nr_kernel_stack 0
    nr_unstable  0
    nr_bounce    0
    nr_vmscan_write 112
    nr_vmscan_immediate_reclaim 0
    nr_writeback_temp 0
    nr_isolated_anon 0
    nr_isolated_file 0
    nr_shmem     0
    nr_dirtied   352
    nr_written   464
    nr_pages_scanned 0
    workingset_refault 0
    workingset_activate 0
    workingset_nodereclaim 0
    nr_anon_transparent_hugepages 0
    nr_free_cma  0
        protection: (0, 1928, 1928, 1928)
  pagesets
    cpu: 0
              count: 0
              high:  0
              batch: 1
  vm stats threshold: 4
    cpu: 1
              count: 0
              high:  0
              batch: 1
  vm stats threshold: 4
  all_unreclaimable: 1
  start_pfn:         1
  inactive_ratio:    1
Node 0, zone    DMA32
  pages free     56774
        min      22348
        low      27935
        high     33522
        scanned  0
        spanned  506914
        present  506658
        managed  494688
    nr_free_pages 56774
    nr_alloc_batch 3023
    nr_inactive_anon 48059
    nr_active_anon 138044
    nr_inactive_file 47952
    nr_active_file 49885
    nr_unevictable 0
    nr_mlock     0
    nr_anon_pages 170349
    nr_mapped    62610
    nr_file_pages 147990
    nr_dirty     231
    nr_writeback 0
    nr_slab_reclaimable 10414
    nr_slab_unreclaimable 16112
    nr_page_table_pages 9231
    nr_kernel_stack 981
    nr_unstable  0
    nr_bounce    0
    nr_vmscan_write 60554686
    nr_vmscan_immediate_reclaim 29587
    nr_writeback_temp 0
    nr_isolated_anon 0
    nr_isolated_file 0
    nr_shmem     8982
    nr_dirtied   3918108
    nr_written   64443856
    nr_pages_scanned 0
    workingset_refault 16084171
    workingset_activate 1186426
    workingset_nodereclaim 0
    nr_anon_transparent_hugepages 2
    nr_free_cma  0
        protection: (0, 0, 0, 0)
  pagesets
    cpu: 0
              count: 135
              high:  186
              batch: 31
  vm stats threshold: 20
    cpu: 1
              count: 166
              high:  186
              batch: 31
  vm stats threshold: 20
  all_unreclaimable: 0
  start_pfn:         4096
  inactive_ratio:    3
";
    print_to_path!(&paths.vmstat, "{}", vmstat_content).expect("cannot initialize vmstat");
    print_to_path!(&paths.zoneinfo, "{}", zoneinfo_content).expect("cannot initialize zoneinfo");
    print_to_path!(&paths.available, "{}\n", TESTING_LOW_MEM_HIGH_AVAILABLE)
        .expect("cannot initialize available");
    print_to_path!(&paths.runnables, "0.16 0.18 0.22 4/981 8504")
        .expect("cannot initialize runnables");
    print_to_path!(&paths.tracing_enabled, "1\n").expect("cannot initialize tracing_enabled");
    print_to_path!(&paths.tracing_on, "1\n").expect("cannot initialize tracing_on");
    print_to_path!(&paths.set_ftrace_filter, "").expect("cannot initialize set_ftrace_filter");
    print_to_path!(&paths.current_tracer, "").expect("cannot initialize current_tracer");
    print_to_path!(&paths.low_mem_margin, "{}", TESTING_LOW_MEM_MARGIN)
        .expect("cannot initialize low_mem_margin");
    mkfifo(&paths.trace_pipe).expect("could not make mock trace_pipe");

    print_to_path!(sys_vm.join("min_filelist_kbytes"), "100000\n")
        .expect("cannot initialize min_filelist_kbytes");
    print_to_path!(sys_vm.join("min_free_kbytes"), "80000\n")
        .expect("cannot initialize min_free_kbytes");
    print_to_path!(sys_vm.join("extra_free_kbytes"), "60000\n")
        .expect("cannot initialize extra_free_kbytes");

    mkfifo(&paths.low_mem_device).expect("could not make mock low-mem device");
}
