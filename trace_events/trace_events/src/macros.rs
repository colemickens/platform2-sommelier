// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The usage of `#[cold]` in this module was shown to improve the no-tracing codepaths by about a
// nanosecond, a 30% improvement.

use super::{tracer, Argument, Label, Point, Scope};

#[doc(hidden)]
#[cold]
pub fn __private_cold_trace_instant(
    category: &str,
    name: &str,
    scope: Scope,
    file: &str,
    line: u32,
) {
    tracer().instant(
        Label { name, category },
        Point::now(),
        scope,
        &[Argument::new("file", file), Argument::new("line", line)],
    )
}

/// Helper that combines checking if the category is enabled with writing an instant trace event
/// type.
///
/// The `name` is formatted using the format macro in the second form of this macro.
///
/// # Example
/// ```
/// use trace_events::trace_instant;
///
/// fn instant() {
///     trace_instant!("display", "vblank");
///     trace_instant!("math", "1 + 2 = {}", 1 + 2);
/// }
/// ```
#[macro_export]
macro_rules! trace_instant {
    ($cat:expr, $name:expr) => (
        if $crate::is_category_enabled($cat) {
            $crate::__private_cold_trace_instant($cat, $name, $crate::Scope::Thread, file!(), line!());
        }
    );
    ($cat:expr, $($args:tt)+) => (
        if $crate::is_category_enabled($cat) {
            $crate::__private_cold_trace_instant($cat, &format!($($args)+), $crate::Scope::Thread, file!(), line!());
        }
    )
}

#[doc(hidden)]
#[cold]
pub fn __private_cold_trace_duration(
    category: &str,
    name: &str,
    begin_ts: u64,
    file: &str,
    line: u32,
) {
    let end = Point::now();
    let begin = Point {
        ts: begin_ts,
        ..end
    };
    tracer().duration(
        Label { name, category },
        begin,
        end.ts.saturating_sub(begin.ts),
        &[Argument::new("file", file), Argument::new("line", line)],
    )
}

/// Helper that combines checking if the category is enabled with writing a duration trace event
/// type.
///
/// The given block is executed inside of a closure and the return value is returned by this macro.
/// That means most control flow statements will not work to escape this macro.
///
/// # Example
/// ```
/// use trace_events::trace_duration_block;
///
/// fn duration_block() {
///     let mut value = 5;
///     let owned = Box::new(10);
///     trace_duration_block!("block", "reading from disk", {
///         value = value * value * value + *owned;
///     });
///     assert_eq!(value, 125 + *owned);
/// }
/// ```
#[macro_export]
macro_rules! trace_duration_block {
    ($cat:expr, $name:expr, $block:block) => {{
        let begin_ts = $crate::sys::get_monotonic();
        let mut block_closure = || $block;
        let block_res = block_closure();
        if $crate::is_category_enabled($cat) {
            $crate::__private_cold_trace_duration($cat, $name, begin_ts, file!(), line!())
        }
        block_res
    }};
}

#[doc(hidden)]
#[cold]
pub fn __private_cold_trace_counter(category: &str, name: &str, key: &str, val: u64) {
    tracer().counter(Label { name, category }, Point::now(), &[(key, val)])
}

/// Helper that combines checking if the category is enabled with writing a counter trace event
/// type.
///
/// # Example
/// ```
/// use trace_events::trace_counter;
///
/// fn counter() {
///     trace_counter!("vm", "memory", "free", 22);
/// }
/// ```
#[macro_export]
macro_rules! trace_counter {
    ($cat:expr, $name:expr, $key:expr, $val:expr) => {
        if $crate::is_category_enabled($cat) {
            $crate::__private_cold_trace_counter($cat, $name, $key, ($val) as u64);
        }
    };
}

#[cfg(test)]
mod tests {
    #[test]
    fn instant() {
        trace_instant!("display", "vblank");
    }

    #[test]
    fn instant_format() {
        let counter = 100;
        trace_instant!("display", "vblank #{}", counter);
    }

    #[test]
    fn duration_block() {
        let mut value = 5;
        let owned = Box::new(10);
        trace_duration_block!("block", "reading from disk", {
            value = value * value * value + *owned;
        });
        assert_eq!(value, 125 + *owned);
    }

    #[test]
    fn counter() {
        trace_counter!("vm", "memory", "free", 22);
    }
}
