# libvda

## About

libvda is a library that provides a C API for video decoding as defined in
[libvda.h](./libvda.h).

## Implementations

### [GPU](./gpu)

An implementation that communicates with `GpuArcVideoDecodeAccelerator` running
in the GPU process. Internally, communication is done with libmojo using the
[VideoDecodeAccelerator](https://chromium.googlesource.com/chromium/src.git/+/HEAD/components/arc/common/video_decode_accelerator.mojom) mojo interface.

### [Fake](./fake)
An empty implementation useful for integration testing. Users can initialize
this implementation to see verbose logs when each vda function is called, as
well as receive empty PICTURE_READY events.

## Running unittests

The GPU unit tests require a ChromeOS environment. Run libvda_unittest on a DUT.
