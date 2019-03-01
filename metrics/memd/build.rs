// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern crate protoc_rust;

use std::env;
use std::fs;
use std::io::Write;
use std::path::PathBuf;

use protoc_rust::Customize;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let proto_root = match env::var("SYSROOT") {
        Ok(dir) => PathBuf::from(dir).join("usr/include/chromeos"),
        // Make this work when typing "cargo build" in platform2/metrics/memd
        Err(_) => PathBuf::from("../../system_api"),
    };
    let proto_dir = proto_root.join("dbus/metrics_event");
    let proto_file = proto_dir.join("metrics_event.proto");

    protoc_rust::run(protoc_rust::Args {
        out_dir: out_dir.as_os_str().to_str().unwrap(),
        input: &[&proto_file.as_os_str().to_str().unwrap()],
        includes: &[&proto_dir.as_os_str().to_str().unwrap()],
        customize: Customize {
            ..Default::default()
        },
    })
    .expect("protoc");

    let mut mod_out = fs::File::create(out_dir.join("proto_include.rs")).unwrap();
    writeln!(
        mod_out,
        "#[path = \"{}\"] pub mod plugin_proto;\npub use plugin_proto::*;",
        out_dir.join("metrics_event.rs").display()
    )
    .unwrap();
}
