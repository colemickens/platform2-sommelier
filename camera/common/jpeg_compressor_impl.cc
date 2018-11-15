/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common/jpeg_compressor_impl.h"

#include <memory>

#include <errno.h>
#include <libyuv.h>

#include <base/memory/ptr_util.h>
#include <base/memory/shared_memory.h>
#include "cros-camera/common.h"
#include "cros-camera/jpeg_encode_accelerator.h"

namespace cros {

// The destination manager that can access members in JpegCompressorImpl.
struct destination_mgr {
 public:
  struct jpeg_destination_mgr mgr;
  JpegCompressorImpl* compressor;
};

// static
std::unique_ptr<JpegCompressor> JpegCompressor::GetInstance() {
  return std::make_unique<JpegCompressorImpl>();
}

JpegCompressorImpl::JpegCompressorImpl()
    : hw_encoder_(nullptr),
      hw_encoder_started_(false),
      out_buffer_ptr_(nullptr),
      out_buffer_size_(0),
      out_data_size_(0),
      is_encode_success_(false) {}

JpegCompressorImpl::~JpegCompressorImpl() {}

bool JpegCompressorImpl::CompressImage(const void* image,
                                       int width,
                                       int height,
                                       int quality,
                                       const void* app1_buffer,
                                       uint32_t app1_size,
                                       uint32_t out_buffer_size,
                                       void* out_buffer,
                                       uint32_t* out_data_size,
                                       JpegCompressor::Mode mode) {
  if (width % 8 != 0 || height % 2 != 0) {
    LOGF(ERROR) << "Image size can not be handled: " << width << "x" << height;
    return false;
  }

  if (out_data_size == nullptr || out_buffer == nullptr) {
    LOGF(ERROR) << "Output should not be nullptr";
    return false;
  }

  auto method_used = [&]() -> const char* {
    if (mode != JpegCompressor::Mode::kSwOnly) {
      // Try HW encode.
      uint32_t input_data_size = static_cast<uint32_t>(width * height * 3 / 2);
      if (EncodeHw(static_cast<const uint8_t*>(image), input_data_size, width,
                   height, static_cast<const uint8_t*>(app1_buffer), app1_size,
                   out_buffer_size, out_buffer, out_data_size)) {
        return "hardware";
      }
      if (mode != JpegCompressor::Mode::kHwOnly) {
        LOGF(WARNING) << "Tried HW encode but failed. Fall back to SW encode";
      }
    }

    if (mode != JpegCompressor::Mode::kHwOnly) {
      // Try SW encode.
      if (Encode(image, width, height, quality, app1_buffer, app1_size,
                 out_buffer_size, out_buffer, out_data_size)) {
        return "software";
      }
    }

    return nullptr;
  }();

  if (method_used == nullptr) {
    // TODO(shik): Map mode from enum to string for better readability.
    LOGF(ERROR) << "Failed to compress image with mode = "
                << static_cast<int>(mode);
    return false;
  }

  LOGF(INFO) << "Compressed JPEG with " << method_used << ": "
             << (width * height * 12) / 8 << "[" << width << "x" << height
             << "] -> " << *out_data_size << " bytes";
  return true;
}

bool JpegCompressorImpl::GenerateThumbnail(const void* image,
                                           int image_width,
                                           int image_height,
                                           int thumbnail_width,
                                           int thumbnail_height,
                                           int quality,
                                           uint32_t out_buffer_size,
                                           void* out_buffer,
                                           uint32_t* out_data_size) {
  if (thumbnail_width == 0 || thumbnail_height == 0) {
    LOGF(ERROR) << "Invalid thumbnail resolution " << thumbnail_width << "x"
                << thumbnail_height;
    return false;
  }
  if (thumbnail_width % 8 != 0 || thumbnail_height % 2 != 0) {
    LOGF(ERROR) << "Image size can not be handled: " << thumbnail_width << "x"
                << thumbnail_height;
    return false;
  }

  if (out_data_size == nullptr || out_buffer == nullptr) {
    LOGF(ERROR) << "Output should not be nullptr. ";
    return false;
  }

  // Resize |image| to |thumbnail_width| x |thumbnail_height|.
  std::vector<uint8_t> scaled_buffer;
  size_t y_plane_size = image_width * image_height;
  const uint8_t* y_plane = reinterpret_cast<const uint8_t*>(image);
  const uint8_t* u_plane = y_plane + y_plane_size;
  const uint8_t* v_plane = u_plane + y_plane_size / 4;

  size_t scaled_y_plane_size = thumbnail_width * thumbnail_height;
  scaled_buffer.resize(scaled_y_plane_size * 3 / 2);
  uint8_t* scaled_y_plane = scaled_buffer.data();
  uint8_t* scaled_u_plane = scaled_y_plane + scaled_y_plane_size;
  uint8_t* scaled_v_plane = scaled_u_plane + scaled_y_plane_size / 4;

  int result = libyuv::I420Scale(
      y_plane, image_width, u_plane, image_width / 2, v_plane, image_width / 2,
      image_width, image_height, scaled_y_plane, thumbnail_width,
      scaled_u_plane, thumbnail_width / 2, scaled_v_plane, thumbnail_width / 2,
      thumbnail_width, thumbnail_height, libyuv::kFilterNone);
  if (result != 0) {
    LOGF(ERROR) << "Generate YUV thumbnail failed";
    return false;
  }

  // Compress thumbnail to JPEG. Since thumbnail size is small, SW performs
  // better than HW.
  return CompressImage(scaled_buffer.data(), thumbnail_width, thumbnail_height,
                       quality, nullptr, 0, out_buffer_size, out_buffer,
                       out_data_size, JpegCompressor::Mode::kSwOnly);
}

void JpegCompressorImpl::InitDestination(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  dest->mgr.next_output_byte = dest->compressor->out_buffer_ptr_;
  dest->mgr.free_in_buffer = dest->compressor->out_buffer_size_;
  dest->compressor->is_encode_success_ = true;
}

boolean JpegCompressorImpl::EmptyOutputBuffer(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  dest->mgr.next_output_byte = dest->compressor->out_buffer_ptr_;
  dest->mgr.free_in_buffer = dest->compressor->out_buffer_size_;
  dest->compressor->is_encode_success_ = false;
  // jcmarker.c in libjpeg-turbo will trigger exit(EXIT_FAILURE) if buffer is
  // not enough to fill marker. If we want to solve this failure, we have to
  // override cinfo.err->error_exit. It's too complicated. Therefore, we use a
  // variable |is_encode_success_| to indicate error and always return true
  // here.
  return true;
}

void JpegCompressorImpl::TerminateDestination(j_compress_ptr cinfo) {
  destination_mgr* dest = reinterpret_cast<destination_mgr*>(cinfo->dest);
  dest->compressor->out_data_size_ =
      dest->compressor->out_buffer_size_ - dest->mgr.free_in_buffer;
}

void JpegCompressorImpl::OutputErrorMessage(j_common_ptr cinfo) {
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message)(cinfo, buffer);
  LOGF(ERROR) << buffer;
}

bool JpegCompressorImpl::EncodeHw(const uint8_t* input_buffer,
                                  uint32_t input_buffer_size,
                                  int width,
                                  int height,
                                  const uint8_t* app1_buffer,
                                  uint32_t app1_buffer_size,
                                  uint32_t out_buffer_size,
                                  void* out_buffer,
                                  uint32_t* out_data_size) {
  if (!hw_encoder_) {
    hw_encoder_ = cros::JpegEncodeAccelerator::CreateInstance();
    hw_encoder_started_ = hw_encoder_->Start();
  }

  if (!hw_encoder_ || !hw_encoder_started_) {
    return false;
  }

  // Create SharedMemory for output buffer.
  std::unique_ptr<base::SharedMemory> output_shm =
      base::WrapUnique(new base::SharedMemory);
  if (!output_shm->CreateAndMapAnonymous(out_buffer_size)) {
    LOGF(ERROR) << "CreateAndMapAnonymous for output buffer failed, size="
                << out_buffer_size;
    return false;
  }

  // Utilize HW Jpeg encode through IPC.
  int status = hw_encoder_->EncodeSync(
      -1, input_buffer, input_buffer_size, static_cast<int32_t>(width),
      static_cast<int32_t>(height), app1_buffer, app1_buffer_size,
      output_shm->handle().fd, static_cast<uint32_t>(out_buffer_size),
      out_data_size);
  if (status == cros::JpegEncodeAccelerator::TRY_START_AGAIN) {
    // There might be some mojo errors. We will give a second try.
    LOG(WARNING) << "EncodeSync() returns TRY_START_AGAIN.";
    hw_encoder_started_ = hw_encoder_->Start();
    if (hw_encoder_started_) {
      status = hw_encoder_->EncodeSync(
          -1, input_buffer, input_buffer_size, static_cast<int32_t>(width),
          static_cast<int32_t>(height), app1_buffer, app1_buffer_size,
          output_shm->handle().fd, static_cast<uint32_t>(out_buffer_size),
          out_data_size);
    } else {
      LOGF(ERROR) << "JPEG encode accelerator can't be started.";
    }
  }
  if (status == cros::JpegEncodeAccelerator::ENCODE_OK) {
    memcpy(static_cast<unsigned char*>(out_buffer), output_shm->memory(),
           *out_data_size);
    return true;
  } else {
    LOGF(ERROR) << "HW encode failed with " << status;
  }

  return false;
}

bool JpegCompressorImpl::Encode(const void* inYuv,
                                int width,
                                int height,
                                int jpeg_quality,
                                const void* app1_buffer,
                                unsigned int app1_size,
                                uint32_t out_buffer_size,
                                void* out_buffer,
                                uint32_t* out_data_size) {
  out_buffer_ptr_ = static_cast<JOCTET*>(out_buffer);
  out_buffer_size_ = out_buffer_size;

  jpeg_compress_struct cinfo;
  jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  // Override output_message() to print error log with ALOGE().
  cinfo.err->output_message = &OutputErrorMessage;
  jpeg_create_compress(&cinfo);
  SetJpegDestination(&cinfo);

  SetJpegCompressStruct(width, height, jpeg_quality, &cinfo);
  jpeg_start_compress(&cinfo, TRUE);

  if (app1_buffer != nullptr && app1_size > 0) {
    jpeg_write_marker(&cinfo, JPEG_APP0 + 1,
                      static_cast<const JOCTET*>(app1_buffer), app1_size);
  }

  if (!Compress(&cinfo, static_cast<const uint8_t*>(inYuv))) {
    is_encode_success_ = false;
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  if (is_encode_success_) {
    *out_data_size = out_data_size_;
  }
  return is_encode_success_;
}

void JpegCompressorImpl::SetJpegDestination(jpeg_compress_struct* cinfo) {
  destination_mgr* dest =
      static_cast<struct destination_mgr*>((*cinfo->mem->alloc_small)(
          (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(destination_mgr)));
  dest->compressor = this;
  dest->mgr.init_destination = &InitDestination;
  dest->mgr.empty_output_buffer = &EmptyOutputBuffer;
  dest->mgr.term_destination = &TerminateDestination;
  cinfo->dest = reinterpret_cast<struct jpeg_destination_mgr*>(dest);
}

void JpegCompressorImpl::SetJpegCompressStruct(int width,
                                               int height,
                                               int quality,
                                               jpeg_compress_struct* cinfo) {
  cinfo->image_width = width;
  cinfo->image_height = height;
  cinfo->input_components = 3;
  cinfo->in_color_space = JCS_YCbCr;
  jpeg_set_defaults(cinfo);

  jpeg_set_quality(cinfo, quality, TRUE);
  jpeg_set_colorspace(cinfo, JCS_YCbCr);
  cinfo->raw_data_in = TRUE;
  cinfo->dct_method = JDCT_IFAST;

  // Configure sampling factors. The sampling factor is JPEG subsampling 420
  // because the source format is YUV420.
  cinfo->comp_info[0].h_samp_factor = 2;
  cinfo->comp_info[0].v_samp_factor = 2;
  cinfo->comp_info[1].h_samp_factor = 1;
  cinfo->comp_info[1].v_samp_factor = 1;
  cinfo->comp_info[2].h_samp_factor = 1;
  cinfo->comp_info[2].v_samp_factor = 1;
}

bool JpegCompressorImpl::Compress(jpeg_compress_struct* cinfo,
                                  const uint8_t* yuv) {
  JSAMPROW y[kCompressBatchSize];
  JSAMPROW cb[kCompressBatchSize / 2];
  JSAMPROW cr[kCompressBatchSize / 2];
  JSAMPARRAY planes[3]{y, cb, cr};

  size_t y_plane_size = cinfo->image_width * cinfo->image_height;
  size_t uv_plane_size = y_plane_size / 4;
  uint8_t* y_plane = const_cast<uint8_t*>(yuv);
  uint8_t* u_plane = const_cast<uint8_t*>(yuv + y_plane_size);
  uint8_t* v_plane = const_cast<uint8_t*>(yuv + y_plane_size + uv_plane_size);
  std::unique_ptr<uint8_t[]> empty(new uint8_t[cinfo->image_width]);
  memset(empty.get(), 0, cinfo->image_width);

  while (cinfo->next_scanline < cinfo->image_height) {
    for (int i = 0; i < kCompressBatchSize; ++i) {
      size_t scanline = cinfo->next_scanline + i;
      if (scanline < cinfo->image_height) {
        y[i] = y_plane + scanline * cinfo->image_width;
      } else {
        y[i] = empty.get();
      }
    }
    // cb, cr only have half scanlines
    for (int i = 0; i < kCompressBatchSize / 2; ++i) {
      size_t scanline = cinfo->next_scanline / 2 + i;
      if (scanline < cinfo->image_height / 2) {
        int offset = scanline * (cinfo->image_width / 2);
        cb[i] = u_plane + offset;
        cr[i] = v_plane + offset;
      } else {
        cb[i] = cr[i] = empty.get();
      }
    }

    int processed = jpeg_write_raw_data(cinfo, planes, kCompressBatchSize);
    if (processed != kCompressBatchSize) {
      LOGF(ERROR) << "Number of processed lines does not equal input lines.";
      return false;
    }
  }
  return true;
}

}  // namespace cros
