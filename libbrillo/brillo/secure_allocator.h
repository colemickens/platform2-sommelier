// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_BRILLO_SECURE_ALLOCATOR_H_
#define LIBBRILLO_BRILLO_SECURE_ALLOCATOR_H_

#include <memory>

#include <brillo/brillo_export.h>

namespace brillo {
// SecureAllocator is a stateless derivation of std::allocator that clears
// the contents of the object on deallocation.
template <typename T>
class BRILLO_PRIVATE SecureAllocator : public std::allocator<T> {
 public:
  using typename std::allocator<T>::pointer;
  using typename std::allocator<T>::size_type;
  using typename std::allocator<T>::value_type;

  // Implicit std::allocator constructors.

  template <typename U> struct rebind {
    typedef SecureAllocator<U> other;
  };

  // Allocation/deallocation: use the std::allocation functions but make sure
  // that on deallocation, the contents of the element are cleared out.
  pointer allocate(size_type  n, pointer = {}) {
    return std::allocator<T>::allocate(n);
  }

  virtual void deallocate(pointer p, size_type n) {
    clear_contents(p, n * sizeof(value_type));
    std::allocator<T>::deallocate(p, n);
  }

 protected:
// Force memset to not be optimized out.
// Original source commit: 31b02653c2560f8331934e879263beda44c6cc76
// Repo: https://android.googlesource.com/platform/external/minijail
#if defined(__clang__)
#define __attribute_no_opt __attribute__((optnone))
#else
#define __attribute_no_opt __attribute__((__optimize__(0)))
#endif

  // Zero-out all bytes in the allocated buffer.
  virtual void __attribute_no_opt clear_contents(pointer v, size_type n) {
    if (!v)
      return;
    memset(v, 0, n);
  }

#undef __attribute_no_opt
};

}  // namespace brillo

#endif  // LIBBRILLO_BRILLO_SECURE_ALLOCATOR_H_
