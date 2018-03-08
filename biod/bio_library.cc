/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "biod/bio_library.h"

#include <utility>

#include <dlfcn.h>

#include <base/logging.h>

namespace biod {

const char* BioAlgorithmTypeToString(BioAlgorithmType type) {
  switch (type) {
    case BioAlgorithmType::kFingerprint:
      return "Fingerprint";
    case BioAlgorithmType::kIris:
      return "Iris";
    default:
      return "Unknown";
  }
}

BioImage::BioImage(BioImage&& rhs) {
  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  image_ = rhs.image_;
  data_ = std::move(rhs.data_);
  rhs.image_ = nullptr;
}

BioImage::~BioImage() {
  Destroy();
}

BioImage& BioImage::operator=(BioImage&& rhs) {
  Destroy();

  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  image_ = rhs.image_;
  data_ = std::move(rhs.data_);
  rhs.image_ = nullptr;

  return *this;
}

bool BioImage::SetData(std::vector<uint8_t>* data) {
  data_ = std::move(*data);
  int ret = lib_->image_set_data_(image_, data_.data(), data_.size());
  if (ret)
    LOG(ERROR) << "Failed to set image data: " << ret;
  return ret == 0;
}

bool BioImage::Destroy() {
  if (!lib_ || !image_)
    return true;

  int ret = lib_->image_destroy_(image_);
  if (ret)
    LOG(ERROR) << "Failed to destroy image: " << ret;

  lib_.reset();
  image_ = nullptr;

  return ret == 0;
}

BioTemplate::BioTemplate(BioTemplate&& rhs) {
  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  template_ = rhs.template_;
  rhs.template_ = nullptr;
}

BioTemplate::~BioTemplate() {
  Destroy();
}

BioTemplate& BioTemplate::operator=(BioTemplate&& rhs) {
  Destroy();
  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  template_ = rhs.template_;
  rhs.template_ = nullptr;

  return *this;
}

int BioTemplate::MatchImage(const BioImage& image) {
  return lib_->template_image_match_(template_, image.get());
}

bool BioTemplate::Serialize(std::vector<uint8_t>* out) {
  ssize_t out_size = lib_->template_get_serialized_size_(template_);
  if (out_size <= 0) {
    LOG(ERROR) << "Failed to get template serialized size: " << out_size;
    return false;
  }
  out->resize(static_cast<size_t>(out_size));

  int ret = lib_->template_serialize_(template_, out->data(), out->size());
  if (ret)
    LOG(ERROR) << "Failed to serialize template: " << ret;

  return ret == 0;
}

bool BioTemplate::Destroy() {
  if (!lib_ || !template_)
    return true;

  int ret = lib_->template_destroy_(template_);
  if (ret)
    LOG(ERROR) << "Failed to destroy template: " << ret;

  lib_.reset();
  template_ = nullptr;

  return ret == 0;
}

BioEnrollment::BioEnrollment(BioEnrollment&& rhs) {
  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  enrollment_ = rhs.enrollment_;
  rhs.enrollment_ = nullptr;
}

BioEnrollment::~BioEnrollment() {
  Destroy();
}

BioEnrollment& BioEnrollment::operator=(BioEnrollment&& rhs) {
  Destroy();

  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  enrollment_ = rhs.enrollment_;
  rhs.enrollment_ = nullptr;

  return *this;
}

int BioEnrollment::AddImage(const BioImage& image) {
  return lib_->enrollment_add_image_(enrollment_, image.get());
}

int BioEnrollment::IsComplete() {
  return lib_->enrollment_is_complete_(enrollment_);
}

int BioEnrollment::GetPercentComplete() {
  return lib_->enrollment_get_percent_complete_(enrollment_);
}

BioTemplate BioEnrollment::Finish() {
  bio_template_t tmplate;
  int ret = lib_->enrollment_finish_(enrollment_, &tmplate);
  if (ret)
    LOG(ERROR) << "Failed to destroy enrollment: " << ret;

  BioTemplate bio_template = BioTemplate(lib_, tmplate);
  lib_.reset();
  enrollment_ = nullptr;

  return bio_template;
}

bool BioEnrollment::Destroy() {
  if (!lib_ || !enrollment_)
    return true;

  int ret = lib_->enrollment_finish_(enrollment_, nullptr /* no template */);
  if (ret)
    LOG(ERROR) << "Failed to destroy enrollment: " << ret;

  lib_.reset();
  enrollment_ = nullptr;

  return ret == 0;
}

BioSensor::BioSensor(BioSensor&& rhs) {
  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  sensor_ = rhs.sensor_;
  rhs.sensor_ = nullptr;
}

BioSensor::~BioSensor() {
  Destroy();
}

BioSensor& BioSensor::operator=(BioSensor&& rhs) {
  Destroy();

  // Use swap to avoid atomic ref counting
  lib_.swap(rhs.lib_);

  sensor_ = rhs.sensor_;
  rhs.sensor_ = nullptr;

  return *this;
}

bool BioSensor::SetModel(const Model& model) {
  int ret = lib_->sensor_set_model_(sensor_,
                                    model.vendor_id,
                                    model.product_id,
                                    model.model_id,
                                    model.version);
  if (ret)
    LOG(ERROR) << "Failed to set sensor model: " << ret;
  return ret == 0;
}

bool BioSensor::SetFormat(uint32_t pixel_format) {
  int ret = lib_->sensor_set_format_(sensor_, pixel_format);
  if (ret)
    LOG(ERROR) << "Failed to set sensor format: " << ret;
  return ret == 0;
}

bool BioSensor::SetSize(uint32_t width, uint32_t height) {
  int ret = lib_->sensor_set_size_(sensor_, width, height);
  if (ret)
    LOG(ERROR) << "Failed to set sensor format: " << ret;
  width_ = width;
  height_ = height;
  return ret == 0;
}

BioImage BioSensor::CreateImage() {
  bio_image_t image;
  int ret = lib_->image_create_(sensor_, width_, height_, &image);
  if (ret) {
    LOG(ERROR) << "Failed to create image: " << ret;
    return BioImage();
  }
  ret = lib_->image_set_size_(image, width_, height_);
  if (ret) {
    LOG(ERROR) << "Failed to set image size: " << ret;
    return BioImage();
  }

  return BioImage(lib_, image);
}

BioEnrollment BioSensor::BeginEnrollment() {
  bio_enrollment_t enrollment;
  int ret = lib_->enrollment_begin_(sensor_, &enrollment);
  if (ret) {
    LOG(ERROR) << "Failed to create enrollment: " << ret;
    return BioEnrollment();
  }
  return BioEnrollment(lib_, enrollment);
}

bool BioSensor::Destroy() {
  if (!lib_ || !sensor_)
    return true;

  int ret = lib_->sensor_destroy_(sensor_);
  if (ret)
    LOG(ERROR) << "Failed to destroy sensor: " << ret;

  lib_.reset();
  sensor_ = nullptr;

  return ret == 0;
}

BioLibrary::~BioLibrary() {
  if (!handle_)
    return;

  if (needs_exit_)
    algorithm_exit_();

  int ret = dlclose(handle_);
  if (ret)
    LOG(WARNING) << "Failed to close bio alogorithm library: " << ret;
}

std::shared_ptr<BioLibrary> BioLibrary::Load(const base::FilePath& path) {
  // Because BioLibrary has a private constructor, make_shared needs a little
  // workaround to construct a BioLibrary. We prefer using make_shared to using
  // the new keyword because make_shared can potentially allocate the control
  // block in the same hunk of memory as the BioLibrary.
  class BioLibraryShared : public BioLibrary {};
  std::shared_ptr<BioLibrary> lib = std::make_shared<BioLibraryShared>();
  if (lib->Init(path))
    return lib;
  return nullptr;
}

BioAlgorithmType BioLibrary::GetAlgorithmType() {
  switch (algorithm_get_type_()) {
    case BIO_ALGORITHM_FINGERPRINT:
      return BioAlgorithmType::kFingerprint;
    case BIO_ALGORITHM_IRIS:
      return BioAlgorithmType::kIris;
    default:
      return BioAlgorithmType::kUnknown;
  }
}

base::StringPiece BioLibrary::GetAlgorithmName() {
  return algorithm_get_name_();
}

base::StringPiece BioLibrary::GetAlgorithmVersion() {
  return algorithm_get_version_();
}

base::StringPiece BioLibrary::GetAlgorithmBanner() {
  return algorithm_get_banner_();
}

BioSensor BioLibrary::CreateSensor() {
  bio_sensor_t sensor;
  int ret = sensor_create_(&sensor);
  if (ret) {
    LOG(ERROR) << "Failed to create sensor: " << ret;
    return BioSensor();
  }
  return BioSensor(shared_from_this(), sensor);
}

BioTemplate BioLibrary::DeserializeTemplate(const std::vector<uint8_t>& data) {
  bio_template_t tmplate;
  int ret = template_deserialize_(data.data(), data.size(), &tmplate);
  if (ret) {
    LOG(ERROR) << "Failed to deserialize template: " << ret;
    return BioTemplate();
  }
  return BioTemplate(shared_from_this(), tmplate);
}

bool BioLibrary::Init(const base::FilePath& path) {
  // Use RTLD_NOW here because it would be better to fail now if there are any
  // unresolved symbols then some random point later on in the usage of this
  // library.
  handle_ = dlopen(path.value().c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle_ == NULL) {
    LOG(ERROR) << "Failed to load bio library from " << path.value() << ": "
               << dlerror();
    return false;
  }

#define BIO_DLSYM(x)                                                  \
  do {                                                                \
    x##_ = reinterpret_cast<bio_##x##_fp>(dlsym(handle_, "bio_" #x)); \
    if (!x##_) {                                                      \
      LOG(ERROR) << "bio_" #x " is missing from library";             \
      return false;                                                   \
    }                                                                 \
  } while (0)

#define BIO_DLSYM_OPTIONAL(x)                                         \
  do {                                                                \
    x##_ = reinterpret_cast<bio_##x##_fp>(dlsym(handle_, "bio_" #x)); \
  } while (0)

  BIO_DLSYM(algorithm_init);
  BIO_DLSYM(algorithm_exit);
  BIO_DLSYM(algorithm_get_type);
  BIO_DLSYM(algorithm_get_name);
  BIO_DLSYM(algorithm_get_version);
  BIO_DLSYM(algorithm_get_banner);
  BIO_DLSYM(sensor_create);
  BIO_DLSYM(sensor_destroy);
  BIO_DLSYM(sensor_set_model);
  BIO_DLSYM(sensor_set_format);
  BIO_DLSYM(sensor_set_size);
  BIO_DLSYM(image_create);
  BIO_DLSYM(image_set_size);
  BIO_DLSYM(image_set_data);
  BIO_DLSYM(image_destroy);
  BIO_DLSYM(template_image_match);
  BIO_DLSYM(template_deserialize);
  BIO_DLSYM(template_get_serialized_size);
  BIO_DLSYM(template_serialize);
  BIO_DLSYM(template_destroy);
  BIO_DLSYM(enrollment_begin);
  BIO_DLSYM(enrollment_add_image);
  BIO_DLSYM(enrollment_is_complete);
  BIO_DLSYM_OPTIONAL(enrollment_get_percent_complete);
  BIO_DLSYM(enrollment_finish);

#undef BIO_DLSYM_OPTIONAL
#undef BIO_DLSYM

  int ret = algorithm_init_();
  if (ret) {
    LOG(ERROR) << "Failed to init bio algorithm library: " << ret;
    return false;
  }

  LOG(INFO) << "FPC Algorithm Info ";
  LOG(INFO) << "  Algorithm Type    : "
            << BioAlgorithmTypeToString(GetAlgorithmType());
  LOG(INFO) << "  Algorithm Name    : " << GetAlgorithmName();
  LOG(INFO) << "  Algorithm Version : " << GetAlgorithmVersion();
  LOG(INFO) << "  Algorithm Banner  : " << GetAlgorithmBanner();

  needs_exit_ = true;

  return true;
}

void* BioLibrary::GetSymbolPointer(const std::string& sym) const {
  return dlsym(handle_, sym.c_str());
}

}  //  namespace biod
