use criterion::{criterion_group, criterion_main, Criterion};

use json_tracer::JsonTracer;
use trace_events::{disable_category, enable_category, set_tracer_boxed, trace_instant};

fn simple_instant() {
    trace_instant!("nums", "fib");
}

fn format_instant(v: i32) {
    trace_instant!("nums", "value = {}", v);
}

fn category_benchmark(c: &mut Criterion) {
    set_tracer_boxed(Box::new(JsonTracer::new()));
    c.bench_function("no category", |b| b.iter(|| simple_instant()));

    enable_category("nums".to_owned());
    c.bench_function("active category", |b| b.iter(|| simple_instant()));
    disable_category("nums");

    enable_category("unrelated".to_owned());
    c.bench_function("inactive category", |b| b.iter(|| simple_instant()));
    disable_category("unrelated");

    c.bench_function("reset categories", |b| b.iter(|| simple_instant()));
}

fn format_benchmark(c: &mut Criterion) {
    enable_category("nums".to_owned());
    c.bench_function("format instant", |b| b.iter(|| format_instant(20)));
}

criterion_group!(benches, category_benchmark, format_benchmark);
criterion_main!(benches);
