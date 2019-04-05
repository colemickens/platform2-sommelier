// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use trace_events::{Argument, ArgumentValue, Label, Point, Scope, Tracer};

use std::cell::Cell;
use std::fmt::Write as FmtWrite;
use std::fs::File;
use std::io::Write;
use std::os::unix::io::AsRawFd;

use libc::dup2;

thread_local!(static LOCAL_RECORDS: Cell<String> = Cell::new(String::with_capacity(4096 * 4)));

fn write_local_record<F: FnOnce(&mut String)>(
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
) -> (usize, usize) {
    LOCAL_RECORDS.with(|cell| {
        let mut s = cell.take();
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
            let _ = write!(s, "{}", ts);
            s.push_str(",");

            if let Some(tts) = thread_ts {
                s.push_str(r#""tts":"#);
                let _ = write!(s, "{}", tts);
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
        let len_cap = (s.len(), s.capacity());
        cell.set(s);
        len_cap
    })
}

fn flush_local_records<F: FnOnce(&str)>(f: F) {
    LOCAL_RECORDS.with(|cell| {
        let mut s = cell.take();
        f(&s);
        s.clear();
        cell.set(s)
    })
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
    pub fn set_out_file(&mut self, out_file: File) {
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

    /// Flushes thread-locally buffered output to the previously set output file. If a string
    /// reference is provided, a copy of the flushed data will be appended to it. The `flushed`
    /// parameter is most useful for unit testing.
    pub fn flush(&self, flushed: Option<&mut String>) {
        flush_local_records(|s| {
            // There is no desire to propagate or report errors in tracing.
            if let Some(mut f) = self.out_file.as_ref() {
                let _ = f.write(&s.as_bytes());
            }
            if let Some(out) = flushed {
                out.push_str(s);
            }
        });
    }

    fn maybe_flush(&self, (used, capacity): (usize, usize)) {
        if used * 2 > capacity {
            self.flush(None)
        }
    }
}

impl Tracer for JsonTracer {
    fn duration_begin(&self, label: Label, begin: Point, args: &[Argument]) {
        self.maybe_flush(write_local_record('B', label, begin, args, &[], |_| {}));
    }

    fn duration_end(&self, label: Label, end: Point, args: &[Argument]) {
        self.maybe_flush(write_local_record('E', label, end, args, &[], |_| {}));
    }

    fn duration(&self, label: Label, begin: Point, duration: u64, args: &[Argument]) {
        self.maybe_flush(write_local_record('X', label, begin, args, &[], |s| {
            s.push_str(r#""duration":"#);
            let _ = write!(s, "{}", duration);
            s.push_str(r#","#);
        }));
    }

    fn instant(&self, label: Label, instant: Point, scope: Scope, args: &[Argument]) {
        let scope_ty = match scope {
            Scope::Global => 'g',
            Scope::Process => 'p',
            Scope::Thread => 't',
        };
        self.maybe_flush(write_local_record('i', label, instant, args, &[], |s| {
            s.push_str(r#""scope":""#);
            s.push(scope_ty);
            s.push_str(r#"","#);
        }));
    }

    fn counter(&self, label: Label, instant: Point, counters: &[(&str, u64)]) {
        self.maybe_flush(write_local_record(
            'C',
            label,
            instant,
            &[],
            counters,
            |_| {},
        ));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::{json, Value};

    #[test]
    fn duration() {
        let t = JsonTracer::new();
        let label = Label::new("test", "duration");
        let begin = Point::from_timestamp(456);
        let duration = 100;
        t.duration(label, begin, duration, &[]);
        let mut out = String::new();
        t.flush(Some(&mut out));
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(out.trim().trim_matches(',')).unwrap();
        assert_eq!(
            out_val,
            json!({
                "name": label.name,
                "cat": label.category,
                "ph": "X",
                "ts": begin.ts,
                "duration": duration,
                "pid": begin.pid,
                "tid": begin.tid,
                "args": {}
            })
        );
    }

    #[test]
    fn instant() {
        let t = JsonTracer::new();
        let label = Label::new("test", "instant");
        let point = Point::from_timestamp(456);
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
        let mut out = String::new();
        t.flush(Some(&mut out));
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(out.trim().trim_matches(',')).unwrap();
        assert_eq!(
            out_val,
            json!({
                "name": label.name,
                "cat": label.category,
                "ph": "i",
                "ts": point.ts,
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
            })
        );
    }

    #[test]
    fn counter() {
        let t = JsonTracer::new();
        let label = Label::new("test", "duration");
        let instant = Point::from_timestamp(42);
        t.counter(label, instant, &[("blocks", 123), ("ducks", 456)]);
        let mut out = String::new();
        t.flush(Some(&mut out));
        println!("out = {}", out);
        let out_val: Value = serde_json::from_str(out.trim().trim_matches(',')).unwrap();
        assert_eq!(
            out_val,
            json!({
                "name": label.name,
                "cat": label.category,
                "ph": "C",
                "ts": instant.ts,
                "pid": instant.pid,
                "tid": instant.tid,
                "args": {
                    "blocks": 123,
                    "ducks": 456,
                }
            })
        );
    }
}
