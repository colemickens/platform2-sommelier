// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::thread::sleep;
use std::time::Duration;

use trace_events::{
    counter, disable_all, duration, enable_category, instant, set_tracer,
    trivial_tracer::{OwnedLabel, OwnedTraceRecord, TrivialTracer},
    Scope,
};

#[instant(category = "display", global)]
fn vblank_happened() {}

#[instant(category = "signals", process)]
fn handle_sigterm() {}

#[instant(category = "gpu", end)]
fn submit_buffer() -> bool {
    true
}

#[duration(category = "suite", name = "task_long")]
fn long_running_task(a: u32, b: u32) -> u32 {
    sleep(Duration::from_millis(50));
    a + b
}

#[counter]
fn process_a_widget() {}

fn test_instant(t: &TrivialTracer) {
    enable_category("display".to_owned());
    enable_category("signals".to_owned());
    enable_category("gpu".to_owned());
    vblank_happened();
    handle_sigterm();
    assert!(submit_buffer());
    disable_all();

    let mut records = t.take_records();
    assert_eq!(records.len(), 3);
    match records.remove(0) {
        (OwnedLabel { category, name }, OwnedTraceRecord::Instant(_, scope, _)) => {
            assert_eq!(category, "display");
            assert_eq!(name, "vblank_happened");
            assert_eq!(scope, Scope::Global);
        }
        _ => panic!("expected instant record"),
    };
    match records.remove(0) {
        (OwnedLabel { category, name }, OwnedTraceRecord::Instant(_, scope, _)) => {
            assert_eq!(category, "signals");
            assert_eq!(name, "handle_sigterm");
            assert_eq!(scope, Scope::Process);
        }
        _ => panic!("expected instant record"),
    };
    match records.remove(0) {
        (OwnedLabel { category, name }, OwnedTraceRecord::Instant(_, scope, _)) => {
            assert_eq!(category, "gpu");
            assert_eq!(name, "submit_buffer");
            assert_eq!(scope, Default::default());
        }
        _ => panic!("expected instant record"),
    };
}

fn test_duration(t: &TrivialTracer) {
    enable_category("suite".to_owned());
    let x = long_running_task(4, 8);
    assert_eq!(x, 12);
    disable_all();

    let mut records = t.take_records();
    assert_eq!(records.len(), 2);
    let begin_point;
    match records.remove(0) {
        (OwnedLabel { category, name }, OwnedTraceRecord::Begin(point, _)) => {
            assert_eq!(category, "suite");
            assert_eq!(name, "task_long");
            begin_point = point;
        }
        _ => panic!("expected begin record"),
    };

    match records.remove(0) {
        (OwnedLabel { category, name }, OwnedTraceRecord::End(end_point, _)) => {
            assert_eq!(category, "suite");
            assert_eq!(name, "task_long");
            assert!(begin_point.ts < end_point.ts);
            assert_eq!(begin_point.pid, end_point.pid);
            assert_eq!(begin_point.tid, end_point.tid);
        }
        _ => panic!("expected end record"),
    };
}

fn test_counter(t: &TrivialTracer) {
    enable_category("fn_attrs".to_owned());
    process_a_widget();
    process_a_widget();
    disable_all();

    let records = t.take_records();
    assert_eq!(records.len(), 2);
    for (i, record) in records.iter().enumerate() {
        match record {
            (OwnedLabel { category, name }, OwnedTraceRecord::Counter(_, counters)) => {
                assert_eq!(category, module_path!());
                assert_eq!(name, "process_a_widget");
                assert_eq!(counters, &[("calls".to_owned(), (i + 1) as u64)]);
            }
            _ => panic!("expected counter record"),
        }
    }
}

#[test]
fn test_all() {
    let t = Box::leak(Box::new(TrivialTracer::default()));
    set_tracer(t);

    test_instant(t);
    test_duration(t);
    test_counter(t);
}
