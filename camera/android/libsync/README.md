## Android libsync

This is a copy of the libsync from Android goog/nyc-mr1-arc branch:

- Copy include/ and src/sync.c from android/system/core/libsync/
- Copy src/strlcpy.c from android/bionic/libc/upstream-openbsd/lib/libc/string/
- Modify sync.c and strlcpy.c to fix compile errors. The diffs can be found in
  src/diff/
