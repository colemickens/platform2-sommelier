# Trace Events

**A framework for adding trace events to your Rust code.**

---

[TOC]

## Goals
* Performance tracing (how many nanoseconds were spent on a task?)
* Debug tracing (what sequence of events led to this bug?)
* Support usage through `chrome://tracing`
* Easy local workstation tracing
* Low enough overhead to always be built
* Low enough overhead to not affect results when activated
* Ergonomic for common use cases
* Flexible enough to support all trace event types
* Pluggable backends that support common formats

## Example

Here is an example of three functions that take advantage of procedural macros exported by the
`trace_events` crate.

```rust
use trace_events::{counter, duration, instant};

#[instant]
fn handle_mouse_click() {
    // ...
}

#[duration]
fn long_running_task() {
    // ...
}

#[counter]
fn commonly_called_routine() {
    // ...
}
```

The `instant` attribute will trace an `instant` type event. If one considers events to take place on
a timeline, an `instant` event would be a point. The `duration` attribute will trace a `duration`
type event whose beginning is when the function was invoked, and whose end is whenever the function
returns. Continuing the timeline analogy, `duration` events would be a segment of the timeline. The
`counter` attribute is similar to an `instant` type event, but also includes a count of the number
of times the function was invoked.

## Enabling a Tracing Backend

The `trace_events` library is patterned after the [log] crate in `crates.io`. The bulk of "library"
code should simply be using the `trace_events` frontend API that includes using the function
attributes, trace macros, and the global `fn tracer() -> &'static Tracer` function. Code should not
be worrying about how trace events are saved or in what format they are being recorded. On the other
hand, executables that wish to support tracing should be using APIs such as `fn set_tracer(tracer:
&'static Tracer)` and `fn enable_category(category: String)` to set the tracing backend and
activated categories, respectively.


```rust
use trace_events::{set_tracer_boxed, enable_category, trivial_tracer::TrivialTracer};

set_tracer_boxed(Box::new(TrivialTracer::default()));
enable_category("interesting".to_owned());
```

This example initializes the `TrivialTracer`, which is exported by this crate for demonstration and
testing purposes, boxes it, and sets it as the global tracing backend for the current process. The
second line enables the `"interesting"` category. Until `disable_category` or similar function is
called, all trace events for that category will end up being written to the tracing backend. Once a
tracer is set, it may not be changed or deinitialized because its lifetime must be `'static`.

[log]: https://github.com/rust-lang-nursery/log
