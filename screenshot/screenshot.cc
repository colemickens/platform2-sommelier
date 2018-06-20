// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <sys/mman.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include <gbm.h>
#include <png.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"

namespace {

constexpr const char DRM_DEVICE_DIR[] = "/dev/dri";
constexpr const char DRM_DEVICE_GLOB[] = "card?";

struct DrmModeResDeleter {
  void operator()(drmModeRes* resources) {
    drmModeFreeResources(resources);
  }
};
using ScopedDrmModeResPtr = std::unique_ptr<drmModeRes, DrmModeResDeleter>;

struct DrmModeCrtcDeleter {
  void operator()(drmModeCrtc* crtc) {
    drmModeFreeCrtc(crtc);
  }
};
using ScopedDrmModeCrtcPtr = std::unique_ptr<drmModeCrtc, DrmModeCrtcDeleter>;

struct DrmModeFBDeleter {
  void operator()(drmModeFB* fb) {
    drmModeFreeFB(fb);
  }
};
using ScopedDrmModeFBPtr = std::unique_ptr<drmModeFB, DrmModeFBDeleter>;

struct GbmDeviceDeleter {
  void operator()(gbm_device* device) {
    gbm_device_destroy(device);
  }
};
using ScopedGbmDevicePtr = std::unique_ptr<gbm_device, GbmDeviceDeleter>;

struct GbmBoDeleter {
  void operator()(gbm_bo* bo) {
    gbm_bo_destroy(bo);
  }
};
using ScopedGbmBoPtr = std::unique_ptr<gbm_bo, GbmBoDeleter>;

// Utility class to map/unmap GBM BO with RAII.
class GbmBoMap {
 public:
  GbmBoMap(gbm_bo* bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
           size_t plane)
      : bo_(bo) {
    buffer_ = gbm_bo_map(
        bo_, x, y, width, height, GBM_BO_TRANSFER_READ, &stride_, &map_data_,
        plane);
  }

  ~GbmBoMap() {
    if (IsValid())
      gbm_bo_unmap(bo_, map_data_);
  }

  bool IsValid() const { return buffer_ != MAP_FAILED; }

  uint32_t stride() const { return stride_; }
  void* buffer() const { return buffer_; }

 private:
  gbm_bo* const bo_;
  uint32_t stride_ = 0;
  void* map_data_ = nullptr;
  void* buffer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GbmBoMap);
};

// Finds the first valid CRTC and returns it and its associated device file.
std::pair<base::File, ScopedDrmModeCrtcPtr> FindFirstValidCrtc() {
  std::vector<base::FilePath> paths;
  {
    base::FileEnumerator lister(base::FilePath(DRM_DEVICE_DIR),
                                false,
                                base::FileEnumerator::FILES,
                                DRM_DEVICE_GLOB);
    for (base::FilePath name = lister.Next();
         !name.empty();
         name = lister.Next()) {
      paths.push_back(name);
    }
  }
  std::sort(paths.begin(), paths.end());

  for (base::FilePath path : paths) {
    base::File file(
        path,
        base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
    if (!file.IsValid())
      continue;

    ScopedDrmModeResPtr resources(drmModeGetResources(file.GetPlatformFile()));
    if (!resources)
      continue;

    for (int index_crtc = 0;
         index_crtc < resources->count_crtcs;
         ++index_crtc) {
      int32_t crtc_id = resources->crtcs[index_crtc];

      ScopedDrmModeCrtcPtr crtc(
          drmModeGetCrtc(file.GetPlatformFile(), crtc_id));
      if (!crtc || !crtc->mode_valid || crtc->buffer_id == 0)
        continue;

      return std::make_pair(std::move(file), std::move(crtc));
    }
  }

  return std::make_pair(base::File(), ScopedDrmModeCrtcPtr());
}

// Saves a BGRX image on memory as a RGB PNG file.
void SaveAsPng(const char* path, void* data, uint32_t width,
               uint32_t height, uint32_t stride) {
  png_struct* png = png_create_write_struct(
      PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  CHECK(png) << "png_create_write_struct failed";

  png_info* info = png_create_info_struct(png);
  CHECK(info) << "png_create_info_struct failed";

  CHECK_EQ(setjmp(png_jmpbuf(png)), 0) << "PNG encode failed";

  base::ScopedFILE fp(fopen(path, "w"));
  PCHECK(fp) << "Failed to open " << path << " for writing";
  png_init_io(png, fp.get());

  png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  png_set_bgr(png);
  png_set_filler(png, 0, PNG_FILLER_AFTER);

  std::vector<png_byte*> rows(height);
  for (uint32_t i = 0; i < height; ++i)
    rows[i] = static_cast<png_byte*>(data) + stride * i;

  png_write_image(png, rows.data());
  png_write_end(png, nullptr);

  png_destroy_write_struct(&png, &info);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    LOG(ERROR) << "Usage: screenshot path/to/out.png";
    return 1;
  }

  base::File file;
  ScopedDrmModeCrtcPtr crtc;
  std::tie(file, crtc) = FindFirstValidCrtc();
  if (!crtc) {
    LOG(ERROR) << "No valid CRTC found. Is the screen on?";
    return 1;
  }

  ScopedDrmModeFBPtr fb(drmModeGetFB(file.GetPlatformFile(), crtc->buffer_id));
  CHECK(fb) << "drmModeGetFB failed";

  ScopedGbmDevicePtr device(gbm_create_device(file.GetPlatformFile()));
  CHECK(device) << "gbm_create_device failed";

  base::ScopedFD buffer_fd;
  {
    int fd;
    int rv = drmPrimeHandleToFD(file.GetPlatformFile(), fb->handle, 0, &fd);
    CHECK_EQ(rv, 0) << "drmPrimeHandleToFD failed";
    buffer_fd.reset(fd);
  }

  ScopedGbmBoPtr bo;
  {
    gbm_import_fd_data fd_data = {
        buffer_fd.get(),
        fb->width,
        fb->height,
        fb->pitch,
        // TODO(djmk): The buffer format is hardcoded to ARGB8888, we should fix
        // this to query for the frambuffer's format instead.
        GBM_FORMAT_ARGB8888,
    };
    bo.reset(gbm_bo_import(device.get(), GBM_BO_IMPORT_FD, &fd_data,
                           GBM_BO_USE_SCANOUT));
  }
  CHECK(bo) << "gbm_bo_import failed";

  GbmBoMap map(bo.get(), 0, 0, fb->width, fb->height, 0);
  CHECK(map.IsValid()) << "gbm_bo_map failed";

  SaveAsPng(argv[1], map.buffer(), fb->width, fb->height, map.stride());

  return 0;
}
