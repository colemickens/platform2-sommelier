// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![doc(file = "../../README.md")]

//! **A framework for adding trace events to your Rust code.**
//!
//! See the `README.md` from the source repository for more information.
//!
//! # Example
//!
//! This example uses the function attributes to adding tracing to some functions.
//! ```rust
//! use trace_events::{counter, duration, instant};
//!
//! #[instant]
//! fn handle_mouse_click() {
//!     // ...
//! }
//!
//! #[duration]
//! fn long_running_task() {
//!     // ...
//! }
//!
//! #[counter]
//! fn commonly_called_routine() {
//!     // ...
//! }
//! ```
//!
//! This example sets the global tracer and enables tracing on a category.
//!
//! ```rust
//! use trace_events::{set_tracer_boxed, enable_category, trivial_tracer::TrivialTracer};
//!
//! set_tracer_boxed(Box::new(TrivialTracer::default()));
//! enable_category("interesting".to_owned());
//! ```

use std::collections::BTreeSet;
use std::num::NonZeroU64;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicUsize, Ordering};

#[doc(hidden)]
pub mod sys;
use sys::*;

pub mod trivial_tracer;

mod macros;
pub use macros::*;

pub use trace_events_macros::{counter, duration, instant};

/// A generic value used as the value for `Argument`.
#[derive(Copy, Clone, PartialEq, Debug)]
pub enum ArgumentValue<'a> {
    Bool(bool),
    Sint(i64),
    Uint(u64),
    Float(f64),
    String(&'a str),
}

impl<'a> ArgumentValue<'a> {
    /// Converts this value of any lifetime into the same value with 'static lifetime or panics if
    /// it is unable (i.e. `self` is a `String`).
    fn as_static(&self) -> ArgumentValue<'static> {
        use ArgumentValue::*;
        match self {
            Bool(b) => Bool(*b),
            Sint(i) => Sint(*i),
            Uint(i) => Uint(*i),
            Float(f) => Float(*f),
            _ => panic!("argument value is not static"),
        }
    }
}

impl<'a> From<&'a str> for ArgumentValue<'a> {
    fn from(v: &'a str) -> ArgumentValue<'a> {
        ArgumentValue::String(v)
    }
}

macro_rules! impl_from_value {
    ($t:ty, $v:expr, $c:ident) => {
        impl From<$t> for ArgumentValue<'static> {
            fn from(v: $t) -> ArgumentValue<'static> {
                use ArgumentValue::*;
                $v($c::from(v))
            }
        }
    };
}

impl_from_value!(bool, Bool, bool);
impl_from_value!(i8, Sint, i64);
impl_from_value!(i16, Sint, i64);
impl_from_value!(i32, Sint, i64);
impl_from_value!(i64, Sint, i64);
impl_from_value!(u8, Uint, u64);
impl_from_value!(u16, Uint, u64);
impl_from_value!(u32, Uint, u64);
impl_from_value!(u64, Uint, u64);
impl_from_value!(f32, Float, f64);
impl_from_value!(f64, Float, f64);

/// A user-defined argument to be included with the trace event.
#[derive(Clone, PartialEq, Debug)]
pub struct Argument<'a, 'b> {
    /// A generic key associated with the value.
    pub key: &'a str,
    /// A value associated with a trace event.
    pub value: ArgumentValue<'b>,
}

impl<'a, 'b> Argument<'a, 'b> {
    /// Constructs a new `Argument` from a key-value pair.
    pub fn new<V: Into<ArgumentValue<'b>>>(key: &'a str, value: V) -> Argument<'a, 'b> {
        Argument {
            key,
            value: value.into(),
        }
    }
}

/// The category and name associated with a trace event. The `category` is used to filter what
/// events get written, and the `name` is used to identify events in a trace viewer.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub struct Label<'a> {
    pub category: &'a str,
    pub name: &'a str,
}

impl<'a> Label<'a> {
    /// Constructs a new `Label` with the given `category` and `name`.
    pub fn new(category: &'a str, name: &'a str) -> Label<'a> {
        Label { category, name }
    }
}

/// A point in the timeline of events including a monotonic timestamp and the thread/process id of
/// the originating trace event.
#[derive(Copy, Clone, PartialEq, Eq, Debug, Default)]
pub struct Point {
    /// A monotonic timestamp in nanoseconds from an epoch.
    pub ts: u64,
    /// A monotonic timestamp in nanoseconds from an epoch associated with the thread.
    pub thread_ts: Option<NonZeroU64>,
    /// A process id.
    pub pid: u32,
    /// A thread/task id.
    pub tid: u32,
}

impl Point {
    /// Constructs a point for here (current process and thread) and now (current monotonic clock
    /// time).
    pub fn now() -> Point {
        let (pid, tid) = get_pid_tid();
        Point {
            ts: get_monotonic(),
            thread_ts: None,
            pid,
            tid,
        }
    }

    /// Constructs a point for here (current process and thread) and the given timestamp in
    /// nanoseconds.
    pub fn from_timestamp(ts: u64) -> Point {
        let (pid, tid) = get_pid_tid();
        Point {
            ts,
            thread_ts: None,
            pid,
            tid,
        }
    }
}

/// The scope of a trace event.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Scope {
    /// A local thread scope. This is the default.
    Thread,
    /// A process wide scope.
    Process,
    /// A global (or at least system-wide) scope.
    Global,
}

impl Default for Scope {
    fn default() -> Self {
        Scope::Thread
    }
}

/// Trait that trace event generators can use to inform `Tracer` implementors of trace events.
///
/// Filtering is not in any of the methods. Callers of `Tracer` should filter events using
/// `is_category_enabled` before calling any of these methods. Implementors should assume category
/// filtering was already done.
pub trait Tracer: Send + Sync {
    /// Write the beginning of a duration type event.
    fn duration_begin(&self, label: Label, begin: Point, args: &[Argument]) {
        let _ = label;
        let _ = begin;
        let _ = args;
    }

    /// Write the ending of a duration type event.
    fn duration_end(&self, label: Label, end: Point, args: &[Argument]) {
        let _ = label;
        let _ = end;
        let _ = args;
    }

    /// Write a combined begin/end duration type event.
    ///
    /// By default, this will use `duration_begin` and `duration_end` to log the trace event, but
    /// some backends may specialize this as an optimization.
    fn duration(&self, label: Label, begin: Point, duration: u64, args: &[Argument]) {
        self.duration_begin(label, begin, args);
        let end = Point {
            ts: begin.ts.saturating_add(duration),
            ..begin
        };
        self.duration_end(label, end, args);
    }

    /// Write an instant type trace event.
    fn instant(&self, label: Label, instant: Point, scope: Scope, args: &[Argument]) {
        let _ = label;
        let _ = instant;
        let _ = scope;
        let _ = args;
    }

    /// Write a counter type trace event.
    fn counter(&self, label: Label, instant: Point, counters: &[(&str, u64)]) {
        let _ = label;
        let _ = instant;
        let _ = counters;
    }

    // TODO(zachr): metadata, object, async events
}

struct NullTracer;
impl Tracer for NullTracer {}

static STATE: AtomicUsize = AtomicUsize::new(0);
const UNINITIALIZED: usize = 0;
const INITIALIZING: usize = 1;
const INITIALIZED: usize = 2;

static mut TRACER: &'static Tracer = &NullTracer;

/// Returns a reference to the global tracer backend.
///
/// The returned tracer has static lifetime. As such it will always be available. Before
/// `set_tracer` or `set_tracer_boxed` is called, this will return a tracer that does nothing.
pub fn tracer() -> &'static dyn Tracer {
    // Safe because the preconditions of the preconditions of this function.
    unsafe { TRACER }
}

/// Sets the global tracer backend that will be returned by `tracer()`.
///
/// This may only be called once per process and will panic if it is called more than once. Because
/// this takes a reference to a static `Tracer` implementors must be `Send + Sync`.
pub fn set_tracer(tracer: &'static dyn Tracer) {
    match STATE.compare_and_swap(UNINITIALIZED, INITIALIZING, Ordering::SeqCst) {
        UNINITIALIZED => {
            // This statement can only happen once because of the `STATE` variable. it is safe
            // because the previous Tracer was NullTracer (which has static lifetime and will remain
            // valid in case anybody still has a ref).
            unsafe {
                TRACER = tracer;
            }
            STATE.store(INITIALIZED, Ordering::SeqCst);
        }
        INITIALIZING => panic!("global tracer is being initialized already"),
        INITIALIZED => panic!("global tracer has already been initialized"),
        _ => panic!("global tracer state is invalid"),
    }
}

/// A convenience wrapper around `set_tracer` that takes a boxed `Tracer` and "leaks" it to ensure
/// that the `Tracer` has static lifetime.
pub fn set_tracer_boxed(tracer: Box<dyn Tracer>) {
    set_tracer(Box::leak(tracer));
}

/// Can be locked by readers or writers.
const CATEGORY_GUARD_VACANT: usize = 0;
/// Can't be locked by anybody
const CATEGORY_GUARD_WRITING: usize = usize::max_value() / 2 + 1;
/// A guard variable for `ENABLED_CATEGORIES`.
static CATEGORY_GUARD: AtomicUsize = AtomicUsize::new(0);
/// As an optimization for the common case where no categories are enabled, this is null.
static mut ENABLED_CATEGORIES: *mut BTreeSet<String> = null_mut();

/// Spins until a write lock can be obtained. Can be started by concurrent readers.
#[inline(always)]
fn spinlock_write() {
    while CATEGORY_GUARD.compare_and_swap(
        CATEGORY_GUARD_VACANT,
        CATEGORY_GUARD_WRITING,
        Ordering::SeqCst,
    ) != CATEGORY_GUARD_VACANT
    {}
}

/// Spins until a read lock can be obtained. If already in READING state, increments the number of
/// readers.
#[inline(always)]
fn spinlock_read() {
    loop {
        if CATEGORY_GUARD.fetch_add(1, Ordering::SeqCst) & CATEGORY_GUARD_WRITING == 0 {
            return;
        }
        CATEGORY_GUARD.fetch_sub(1, Ordering::SeqCst);
    }
}

/// Unlocks this read lock. Panics if the there was no lock.
#[inline(always)]
fn unlock_read() {
    let old_guard = CATEGORY_GUARD.fetch_sub(1, Ordering::SeqCst);
    if old_guard == 0 {
        panic!("unlock called while lock was not held");
    }
}

/// Unlocks this write lock.
#[inline(always)]
fn unlock_write() {
    CATEGORY_GUARD.store(CATEGORY_GUARD_VACANT, Ordering::SeqCst);
}

/// Call while holding any lock. Returns true if any tracing is enabled for the given category.
#[inline(always)]
unsafe fn locked_is_category_enabled(cat: &str) -> bool {
    (&*ENABLED_CATEGORIES).contains(cat)
}

// Call while holding a write lock. Enables the given category of tracing.
#[inline(always)]
unsafe fn write_locked_enable_category(cat: String) {
    (&mut *ENABLED_CATEGORIES).insert(cat);
}

// Call while holding a write lock. Disables the given category of tracing.
#[inline(always)]
unsafe fn write_locked_disable_category(cat: &str) {
    (&mut *ENABLED_CATEGORIES).remove(cat);
}

/// Enables traces for the given category.
pub fn enable_category(category: String) {
    spinlock_write();
    if unsafe { ENABLED_CATEGORIES }.is_null() {
        unsafe { ENABLED_CATEGORIES = Box::into_raw(Box::new(Default::default())) }
    }
    // Safe because we hold the write lock and ensured ENABLED_CATEGORIES is non-null.
    unsafe { write_locked_enable_category(category) };
    unlock_write();
}

/// Disables traces for the given category.
pub fn disable_category(category: &str) {
    spinlock_write();
    // Note the easy to miss logical inverse (!).
    if !unsafe { ENABLED_CATEGORIES }.is_null() {
        // Safe because we hold the write lock and ensured ENABLED_CATEGORIES is non-null.
        unsafe {
            write_locked_disable_category(category);
        }
        if unsafe { &*ENABLED_CATEGORIES }.is_empty() {
            unsafe {
                Box::from_raw(ENABLED_CATEGORIES);
                ENABLED_CATEGORIES = null_mut();
            }
        }
    }
    unlock_write();
}

/// Disables traces for all previously enabled categories.
pub fn disable_all() {
    spinlock_write();
    // Note the easy to miss logical inverse (!).
    if unsafe { ENABLED_CATEGORIES }.is_null() {
        unsafe {
            Box::from_raw(ENABLED_CATEGORIES);
            ENABLED_CATEGORIES = null_mut();
        }
    }
    unlock_write();
}

/// Returns `true` if the given category is currently enabled.
pub fn is_category_enabled(category: &str) -> bool {
    if unsafe { ENABLED_CATEGORIES }.is_null() {
        return false;
    }
    spinlock_read();
    if unsafe { ENABLED_CATEGORIES }.is_null() {
        return false;
    }
    // Safe because we hold the read lock and ensured ENABLED_CATEGORIES is non-null.
    let enabled = unsafe { locked_is_category_enabled(category) };
    unlock_read();
    enabled
}

#[cfg(test)]
mod tests {
    use super::*;

    use super::trivial_tracer::{OwnedTraceRecord as Record, TrivialTracer};

    use std::thread::sleep;
    use std::time::Duration;

    #[test]
    fn point_now() {
        let a = Point::now();
        sleep(Duration::from_millis(50));
        let b = Point::now();
        assert!(a.ts < b.ts);
        assert_eq!(a.pid, b.pid);
        assert_eq!(a.tid, b.tid);
    }

    #[test]
    fn duration() {
        let t = TrivialTracer::default();
        let l = Label::new("gpu", "composite");
        let begin = Point::from_timestamp(100);
        let duration = 23;
        let end = Point::from_timestamp(begin.ts + duration);
        t.duration(l, begin, duration, &[]);
        let records = t.clone_records();
        assert_eq!(records.len(), 2);
        assert_eq!(
            records[0],
            (l.into(), Record::Begin(begin, Default::default()))
        );
        assert_eq!(records[1], (l.into(), Record::End(end, Default::default())));
    }

    #[test]
    fn duration_begin_end() {
        let t = TrivialTracer::default();
        let l = Label::new("gpu", "composite");
        let begin = Point::from_timestamp(100);
        let end = Point::from_timestamp(123);
        t.duration_begin(l, Point::from_timestamp(100), &[]);
        t.duration_end(l, Point::from_timestamp(123), &[]);
        let records = t.clone_records();
        assert_eq!(records.len(), 2);
        assert_eq!(
            records[0],
            (l.into(), Record::Begin(begin, Default::default()))
        );
        assert_eq!(records[1], (l.into(), Record::End(end, Default::default())));
    }

    #[test]
    fn instant() {
        let t = TrivialTracer::default();
        let l = Label::new("gpu", "vblank");
        let p = Point::from_timestamp(456);
        let s = Scope::Global;
        t.instant(l, p, s, &[]);
        let records = t.clone_records();
        assert_eq!(records.len(), 1);
        assert_eq!(
            records[0],
            (l.into(), Record::Instant(p, s, Default::default()))
        );
    }

    #[test]
    fn counter() {
        let t = TrivialTracer::default();
        let l = Label::new("gpu", "vblank");
        let p = Point::from_timestamp(678);
        t.counter(l, p, &[("cats", 0)]);
        t.counter(l, p, &[("cats", 10)]);
        t.counter(l, p, &[("cats", 3)]);
        let records = t.clone_records();
        assert_eq!(records.len(), 3);
        assert_eq!(
            records[0],
            (l.into(), Record::Counter(p, vec![("cats".to_owned(), 0)]))
        );
        assert_eq!(
            records[1],
            (l.into(), Record::Counter(p, vec![("cats".to_owned(), 10)]))
        );
        assert_eq!(
            records[2],
            (l.into(), Record::Counter(p, vec![("cats".to_owned(), 3)]))
        );
    }
}
