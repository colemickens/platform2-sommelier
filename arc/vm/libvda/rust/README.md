# Libvda Rust wrapper

Rust wrapper for libvda. This library is intended to be used as a back-end of
crosvm's virtio-vdec device.

## Building package

You can build and run unittests with emerge in choot.

```shell
# Build
(chroot)$ emerge-{BOARD} dev-rust/libvda
# Unit tests
(chroot)$ FEATURES=test emerge-{BOARD} dev-rust/libvda
```

You can also execute `cargo` directly for faster build and tests. This would be
useful when you are developing this crate.
Since this crate depends on libvda.so, you need to install it to host
environment first.

```shell
# Change host's linker to gold since host environment's default linker GNU ld
# fails to link libraries.
(chroot)$ sudo binutils-config --linker ld.gold
(chroot)$ sudo emerge chromeos-base/libvda        # Install libvda.so to host.
# Build
(chroot)$ cargo build
# Unit tests
(chroot)$ cargo test
```

## Updating bindings.rs

`src/bindings.rs` is an automatically generated code from `libvda.h`.

```shell
$ cd ${CHROMEOS_DIR}/src/platform2/
$ bindgen arc/vm/libvda/libvda_decode.h -- -I . > arc/vm/libvda/rust/src/bindings.rs
```
