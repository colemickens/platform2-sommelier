// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! A system support module intended for use internally by `trace_events` crate.

use libc::{
    c_long, clock_gettime, clockid_t, syscall, timespec, SYS_getpid, SYS_gettid, CLOCK_MONOTONIC,
};

const NANOS_PER_SEC: u64 = 1_000_000_000;

fn zero_timespec() -> timespec {
    timespec {
        tv_sec: 0,
        tv_nsec: 0,
    }
}

fn get_clock(clock_id: clockid_t) -> u64 {
    let mut ts = zero_timespec();
    // Safe because the `timespec` is the correct size and only needs to last long enough for the
    // syscall to return.
    match unsafe { clock_gettime(clock_id, &mut ts) } {
        0 => (ts.tv_sec as u64) * NANOS_PER_SEC + (ts.tv_nsec as u64),
        _ => 0,
    }
}

/// Returns the system monotonic clock in nanoseconds, or `0` if that clock is unavailable.
pub fn get_monotonic() -> u64 {
    get_clock(CLOCK_MONOTONIC)
}

thread_local! {
    static PID_TID: (u32, u32) = unsafe { (syscall(SYS_getpid as c_long) as u32, syscall(SYS_gettid as c_long) as u32) };
}

/// Returns the `(pid, tid)` of the calling process/thread.
pub fn get_pid_tid() -> (u32, u32) {
    PID_TID.with(|id| *id)
}
