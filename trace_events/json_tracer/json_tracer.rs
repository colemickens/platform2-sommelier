// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use trace_events::{Argument, ArgumentValue, Label, Point, Scope, Tracer};

use std::fmt::Write as FmtWrite;
use std::fs::File;
use std::io::Write;
use std::os::unix::io::AsRawFd;

use libc::dup2;

const NANOS_PER_MICRO: u64 = 1000;

// This will immediately write the record to the given file. The original implementation of this
// function cached records in a thread local buffer, but this complicated the usage of this library
// too significantly. There was no good time to flush the cache without using exotic OS alarm
// primitives or manually putting code to flush in every thread of the client application.
fn write_record<F: FnOnce(&mut String)>(
    ph: char,
    Label { name, category }: Label,
    Point {
        ts,
        thread_ts,
        tid,
        pid,
    }: Point,
    args: &[Argument],
    counters: &[(&str, u64)],
    extra_props: F,
    out_file: Option<&File>,
) {
    let mut f = match out_file {
        Some(f) => f,
        None => return,
    };
    let mut s = String::with_capacity(4096);
    s.push('{');

    {
        s.push_str(r#""name":""#);
        s.push_str(name);
        s.push_str(r#"","#);

        s.push_str(r#""cat":""#);
        s.push_str(category);
        s.push_str(r#"","#);

        s.push_str(r#""ph":""#);
        s.push(ph);
        s.push_str(r#"","#);

        s.push_str(r#""ts":"#);
        let _ = write!(s, "{}", ts / NANOS_PER_MICRO);
        s.push_str(",");

        if let Some(tts) = thread_ts {
            s.push_str(r#""tts":"#);
            let _ = write!(s, "{}", tts.get() / NANOS_PER_MICRO);
            s.push_str(",");
        }

        s.push_str(r#""pid":"#);
        let _ = write!(s, "{}", pid);
        s.push_str(",");

        s.push_str(r#""tid":"#);
        let _ = write!(s, "{}", tid);
        s.push_str(",");

        extra_props(&mut s);

        s.push_str(r#""args":{"#);
        {
            for arg in args {
                s.push('"');
                s.push_str(arg.key);
                s.push_str(r#"":"#);
                match arg.value {
                    ArgumentValue::Bool(b) => s.push_str(if b { "true" } else { "false" }),
                    ArgumentValue::Sint(i) => {
                        let _ = write!(s, "{}", i);
                    }
                    ArgumentValue::Uint(i) => {
                        let _ = write!(s, "{}", i);
                    }
                    ArgumentValue::Float(f) => {
                        let _ = write!(s, "{}", f);
                    }
                    ArgumentValue::String(v) => {
                        let _ = write!(s, "{:?}", v);
                    }
                }
                s.push(',');
            }
            for (key, value) in counters {
                s.push('"');
                s.push_str(key);
                s.push_str(r#"":"#);
                let _ = write!(s, "{}", value);
                s.push(',');
            }
            // Remove trailing comma
            if !args.is_empty() || !counters.is_empty() {
                s.pop();
            }
        }
        s.push_str("}");
    }

    s.push_str("},\n");
    let _ = f.write(s.as_bytes());
}

/// A tracer that outputs to a file in the [Json Trace Event Format](https://docs.google.com/document/u/1/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/view).
///
/// The most convenient viewer is the one builtin to Chrome: `chrome://tracing`.
#[derive(Default)]
pub struct JsonTracer {
    out_file: Option<File>,
}

impl JsonTracer {
    /// Creates a new `JsonTracer` with no output.
    pub fn new() -> JsonTracer {
        Default::default()
    }

    /// Sets the file that tracing output will be written to.
    ///
    /// The given file will immediately start being written to in order to output prefix data.
    pub fn set_out_file(&mut self, mut out_file: File) {
        let _ = out_file.write(b"[\n");
        self.out_file = Some(out_file);
    }

    /// Closes the previously set file and outputs to the given `out_file`, returning `true` on
    /// success.
    ///
    /// This will fail if `set_out_file` was never called previously. Internally, this uses `dup2`
    /// to atomically clone the given file to the already set file, allowing `JsonTracer` to be
    /// lock-free.
    ///
    /// This is most useful for closing the previously set file after tracing is finished while
    /// still allowing any outstanding flushes to safely write to `out_file`, often `/dev/null`.
    pub fn reset_out_file(&self, out_file: &File) -> bool {
        match self.out_file.as_ref() {
            Some(f) => unsafe { dup2(out_file.as_raw_fd(), f.as_raw_fd()) == 0 },
            None => false,
        }
    }
}

impl Tracer for JsonTracer {
    fn duration_begin(&self, label: Label, begin: Point, args: &[Argument]) {
        write_record('B', label, begin, args, &[], |_| {}, self.out_file.as_ref());
    }

    fn duration_end(&self, label: Label, end: Point, args: &[Argument]) {
        write_record('E', label, end, args, &[], |_| {}, self.out_file.as_ref());
    }

    fn duration(&self, label: Label, begin: Point, duration: u64, args: &[Argument]) {
        write_record(
            'X',
            label,
            begin,
            args,
            &[],
            |s| {
                s.push_str(r#""dur":"#);
                let _ = write!(s, "{}", duration / NANOS_PER_MICRO);
                s.push_str(r#","#);
            },
            self.out_file.as_ref(),
        );
    }

    fn instant(&self, label: Label, instant: Point, scope: Scope, args: &[Argument]) {
        let scope_ty = match scope {
            Scope::Global => 'g',
            Scope::Process => 'p',
            Scope::Thread => 't',
        };
        write_record(
            'i',
            label,
            instant,
            args,
            &[],
            |s| {
                s.push_str(r#""scope":""#);
                s.push(scope_ty);
                s.push_str(r#"","#);
            },
            self.out_file.as_ref(),
        );
    }

    fn counter(&self, label: Label, instant: Point, counters: &[(&str, u64)]) {
        write_record(
            'C',
            label,
            instant,
            &[],
            counters,
            |_| {},
            self.out_file.as_ref(),
        );
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use libc::{c_char, syscall, SYS_memfd_create};
    use serde_json::{json, Value};
    use std::fs::File;
    use std::io::{Read, Seek, SeekFrom};
    use std::os::unix::io::{FromRawFd, RawFd};

    /// Creates a pair of memfds pointing to the same backing.
    fn memfd_create_pair() -> (File, File) {
        let f1 = unsafe {
            let fd = syscall(
                SYS_memfd_create,
                b"/json_memfd\0".as_ptr() as *const c_char,
                0,
            );
            assert!(fd >= 0, "failed to create memfd");
            File::from_raw_fd(fd as RawFd)
        };
        let f2 = f1.try_clone().expect("failed to clone memfd");
        (f1, f2)
    }

    /// Reads the contents of a file prior to its current cursor and terminates the trace for parsing.
    fn read_out(mut f: &File) -> String {
        let cursor = f
            .seek(SeekFrom::Current(0))
            .expect("failed to find file cursor");
        let mut s = String::with_capacity(cursor as usize);
        f.seek(SeekFrom::Start(0))
            .expect("failed to seek file cursor to beginning");
        f.read_to_string(&mut s)
            .expect("failed to read file to string");
        s.truncate(s.len() - 2);
        s.push(']');
        s
    }

    /// Creates a `JsonTracer` with its output set to a file, along with a duplication of that file
    /// intended for reading later.
    fn init_json_tracer() -> (JsonTracer, File) {
        let (f1, f2) = memfd_create_pair();
        let mut t = JsonTracer::new();
        t.set_out_file(f1);
        (t, f2)
    }

    #[test]
    fn duration() {
        let (t, out_file) = init_json_tracer();
        let label = Label::new("test", "duration");
        let begin = Point::from_timestamp(45600);
        let duration = 10000;
        t.duration(label, begin, duration, &[]);
        let out = read_out(&out_file);
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(&out).unwrap();
        assert_eq!(
            out_val,
            json!([{
                "name": label.name,
                "cat": label.category,
                "ph": "X",
                "ts": begin.ts / NANOS_PER_MICRO,
                "dur": duration / NANOS_PER_MICRO,
                "pid": begin.pid,
                "tid": begin.tid,
                "args": {}
            }])
        );
    }

    #[test]
    fn instant() {
        let (t, out_file) = init_json_tracer();
        let label = Label::new("test", "instant");
        let point = Point::from_timestamp(45600);
        let scope = Scope::Global;
        t.instant(
            label,
            point,
            scope,
            &[
                Argument::new("apples", 2),
                Argument::new("cost", 5.1),
                Argument::new("paid", true),
                Argument::new("invalid", false),
                Argument::new("special", "spicy"),
            ],
        );
        let out = read_out(&out_file);
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(&out).unwrap();
        assert_eq!(
            out_val,
            json!([{
                "name": label.name,
                "cat": label.category,
                "ph": "i",
                "ts": point.ts / NANOS_PER_MICRO,
                "pid": point.pid,
                "tid": point.tid,
                "scope": "g",
                "args": {
                    "apples": 2,
                    "cost": 5.1,
                    "paid": true,
                    "invalid": false,
                    "special": "spicy",
                }
            }])
        );
    }

    #[test]
    fn counter() {
        let (t, out_file) = init_json_tracer();
        let label = Label::new("test", "duration");
        let instant = Point::from_timestamp(420000);
        t.counter(label, instant, &[("blocks", 123), ("ducks", 456)]);
        let out = read_out(&out_file);
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(&out).unwrap();
        assert_eq!(
            out_val,
            json!([{
                "name": label.name,
                "cat": label.category,
                "ph": "C",
                "ts": instant.ts / NANOS_PER_MICRO,
                "pid": instant.pid,
                "tid": instant.tid,
                "args": {
                    "blocks": 123,
                    "ducks": 456,
                }
            }])
        );
    }
}
