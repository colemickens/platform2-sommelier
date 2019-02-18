# `graphics_tests`

This directory contains files that are installed to test system images so they
can be used by graphics-focused [Tast] integration tests.

Source files' names take the form `graphics.TestName.bin_name.c`. They are
compiled to e.g. `graphics.TestName.bin_name` and installed to
`/usr/local/libexec/graphics_tests` in test system images.

Please do not add new files to this directory if you can avoid it. Spreading a
test's logic across multiple locations makes it harder to understand and to
modify.

[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/