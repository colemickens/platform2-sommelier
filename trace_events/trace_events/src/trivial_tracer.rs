// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module contains the testing and demonstration `TrivialTracer` and associated types.

use std::collections::BTreeMap;
use std::sync::Mutex;

use super::{Argument, ArgumentValue, Label, Point, Scope, Tracer};

/// The value associated with a key in `OwnedArgument`.
#[derive(Clone, PartialEq, Debug)]
pub enum OwnedArgumentValue {
    Static(ArgumentValue<'static>),
    String(String),
}

impl<'a> From<ArgumentValue<'a>> for OwnedArgumentValue {
    fn from(v: ArgumentValue<'a>) -> Self {
        match v {
            ArgumentValue::String(s) => OwnedArgumentValue::String(s.to_owned()),
            inner => OwnedArgumentValue::Static(inner.as_static()),
        }
    }
}

/// A single key-value pair for a trace event record.
#[derive(Clone, PartialEq, Debug, Default)]
pub struct OwnedArgument {
    args: BTreeMap<String, OwnedArgumentValue>,
}

impl From<&[Argument<'_, '_>]> for OwnedArgument {
    fn from(args: &[Argument]) -> Self {
        let args = args
            .iter()
            .map(|a| (a.key.to_owned(), OwnedArgumentValue::from(a.value)))
            .collect();
        OwnedArgument { args }
    }
}

/// The category and name of a trace event record.
#[derive(Clone, PartialEq, Debug)]
pub struct OwnedLabel {
    pub category: String,
    pub name: String,
}

impl<'a> From<Label<'a>> for OwnedLabel {
    fn from(v: Label) -> Self {
        OwnedLabel {
            category: v.category.to_owned(),
            name: v.name.to_owned(),
        }
    }
}

/// A record of a single trace event of any type.
#[derive(Clone, PartialEq, Debug)]
pub enum OwnedTraceRecord {
    /// The beginning of a duration type event.
    Begin(Point, OwnedArgument),
    /// The ending of a duration type event.
    End(Point, OwnedArgument),
    /// An instant type event.
    Instant(Point, Scope, OwnedArgument),
    /// A counter type event, along with counters.
    Counter(Point, Vec<(String, u64)>),
}

/// A simple recording tracer intended for testing and demonstration.
#[derive(Default)]
pub struct TrivialTracer {
    records: Mutex<Vec<(OwnedLabel, OwnedTraceRecord)>>,
}

impl TrivialTracer {
    /// Clones the record of trace events up to this point or since the last call to `take_records`.
    pub fn clone_records(&self) -> Vec<(OwnedLabel, OwnedTraceRecord)> {
        self.records.lock().unwrap().clone()
    }

    /// Returns the record of trace events since the last call to `take_records`.
    pub fn take_records(&self) -> Vec<(OwnedLabel, OwnedTraceRecord)> {
        self.records.lock().unwrap().split_off(0)
    }

    fn push(&self, label: Label, record: OwnedTraceRecord) {
        self.records.lock().unwrap().push((label.into(), record))
    }
}

impl Tracer for TrivialTracer {
    fn duration_begin(&self, label: Label, begin: Point, args: &[Argument]) {
        self.push(label, OwnedTraceRecord::Begin(begin, args.into()));
    }

    fn duration_end(&self, label: Label, end: Point, args: &[Argument]) {
        self.push(label, OwnedTraceRecord::End(end, args.into()));
    }

    fn instant(&self, label: Label, instant: Point, scope: Scope, args: &[Argument]) {
        self.push(
            label,
            OwnedTraceRecord::Instant(instant, scope, args.into()),
        );
    }

    fn counter(&self, label: Label, instant: Point, counters: &[(&str, u64)]) {
        let counters = counters.iter().map(|&(k, v)| (k.to_owned(), v)).collect();
        self.push(label, OwnedTraceRecord::Counter(instant, counters));
    }
}
