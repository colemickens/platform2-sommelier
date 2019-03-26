// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use super::*;

#[test]
fn path_joins() {
    let root = PathBuf::from("/a/b/c");
    let path = PathBuf::from("/a/b/c/d/e/f");

    assert_eq!(
        &join_path(path.clone(), "nested", &root).expect("normal"),
        Path::new("/a/b/c/d/e/f/nested")
    );

    let p1 = join_path(path.clone(), "..", &root).expect("parent 1");
    assert_eq!(&p1, Path::new("/a/b/c/d/e/"));

    let p2 = join_path(p1, "..", &root).expect("parent 2");
    assert_eq!(&p2, Path::new("/a/b/c/d/"));

    let p3 = join_path(p2, "..", &root).expect("parent 3");
    assert_eq!(&p3, Path::new("/a/b/c/"));

    let p4 = join_path(p3, "..", &root).expect("parent of root");
    assert_eq!(&p4, Path::new("/a/b/c/"));
}

#[test]
fn invalid_joins() {
    let root = PathBuf::from("/a");
    let path = PathBuf::from("/a/b");

    join_path(path.clone(), ".", &root).expect_err("current directory");
    join_path(path.clone(), "c/d/e", &root).expect_err("too many components");
    join_path(path.clone(), "/c/d/e", &root).expect_err("absolute path");
}
