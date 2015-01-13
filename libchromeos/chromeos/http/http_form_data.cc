// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_form_data.h>

#include <limits>

#include <base/format_macros.h>
#include <base/rand_util.h>
#include <base/strings/stringprintf.h>

#include <chromeos/errors/error_codes.h>
#include <chromeos/http/http_transport.h>
#include <chromeos/mime_utils.h>

namespace chromeos {
namespace http {

namespace form_header {
const char kContentDisposition[] = "Content-Disposition";
const char kContentTransferEncoding[] = "Content-Transfer-Encoding";
const char kContentType[] = "Content-Type";
}  // namespace form_header

const char content_disposition::kFile[] = "file";
const char content_disposition::kFormData[] = "form-data";

FormField::FormField(const std::string& name,
                     const std::string& content_disposition,
                     const std::string& content_type,
                     const std::string& transfer_encoding)
    : name_{name},
      content_disposition_{content_disposition},
      content_type_{content_type},
      transfer_encoding_{transfer_encoding} {
}

std::string FormField::GetContentDisposition() const {
  std::string disposition = content_disposition_;
  if (!name_.empty())
    base::StringAppendF(&disposition, "; name=\"%s\"", name_.c_str());
  return disposition;
}

std::string FormField::GetContentType() const {
  return content_type_;
}

std::string FormField::GetContentHeader() const {
  HeaderList headers{
      {form_header::kContentDisposition, GetContentDisposition()}
  };

  if (!content_type_.empty())
    headers.emplace_back(form_header::kContentType, GetContentType());

  if (!transfer_encoding_.empty()) {
    headers.emplace_back(form_header::kContentTransferEncoding,
                         transfer_encoding_);
  }

  std::string result;
  for (const auto& pair : headers) {
    base::StringAppendF(
        &result, "%s: %s\r\n", pair.first.c_str(), pair.second.c_str());
  }
  result += "\r\n";
  return result;
}

TextFormField::TextFormField(const std::string& name,
                             const std::string& data,
                             const std::string& content_type,
                             const std::string& transfer_encoding)
    : FormField{name,
                content_disposition::kFormData,
                content_type,
                transfer_encoding},
      data_{data} {
}

uint64_t TextFormField::GetDataSize() const {
  return data_.GetDataSize();
}

bool TextFormField::ReadData(void* buffer,
                             size_t size_to_read,
                             size_t* size_read,
                             chromeos::ErrorPtr* error) {
  return data_.ReadData(buffer, size_to_read, size_read, error);
}

FileFormField::FileFormField(const std::string& name,
                             base::File file,
                             const std::string& file_name,
                             const std::string& content_disposition,
                             const std::string& content_type,
                             const std::string& transfer_encoding)
    : FormField{name, content_disposition, content_type, transfer_encoding},
      file_{file.Pass()},
      file_name_{file_name} {
}

uint64_t FileFormField::GetDataSize() const {
  // Unfortunately base::File::GetLength() is not a const method of the class,
  // for no apparent reason, so have to const_cast the file reference.
  // TODO(avakulenko): Remove this cast once the base::File::GetLength() is
  // fixed. See crbug.com/446528
  int64_t size = const_cast<base::File&>(file_).GetLength();
  return size < 0 ? 0 : size;
}

std::string FileFormField::GetContentDisposition() const {
  std::string disposition = FormField::GetContentDisposition();
  base::StringAppendF(&disposition, "; filename=\"%s\"", file_name_.c_str());
  return disposition;
}

bool FileFormField::ReadData(void* buffer,
                             size_t size_to_read,
                             size_t* size_read,
                             chromeos::ErrorPtr* error) {
  // base::File uses 'int' for sizes. On 64 bit systems size_t can be larger
  // than what 'int' can contain, so limit the reading to the max size.
  // This is acceptable because ReadData is not guaranteed to read all the data
  // at once. The caller is expected to keep calling this function until all
  // the data have been read.
  const size_t max_size = std::numeric_limits<int>::max();
  if (size_to_read > max_size)
    size_to_read = max_size;

  int read = file_.ReadAtCurrentPosNoBestEffort(reinterpret_cast<char*>(buffer),
                                                size_to_read);
  if (read < 0) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    return false;
  }
  *size_read = read;
  return true;
}

MultiPartFormField::MultiPartFormField(const std::string& name,
                                       const std::string& content_type,
                                       const std::string& boundary)
    : FormField{name,
                content_disposition::kFormData,
                content_type.empty() ? mime::multipart::kMixed : content_type,
                {}},
      boundary_{boundary} {
  if (boundary_.empty())
    boundary_ = base::StringPrintf("%016" PRIx64, base::RandUint64());
}

uint64_t MultiPartFormField::GetDataSize() const {
  uint64_t boundary_size = GetBoundaryStart().size();
  uint64_t size = 0;
  for (const auto& part : parts_) {
    size += boundary_size;
    size += part->GetContentHeader().size();
    size += part->GetDataSize();
    size += 2;  // CRLF
  }
  if (!parts_.empty())
    size += GetBoundaryEnd().size();
  return size;
}

std::string MultiPartFormField::GetContentType() const {
  return base::StringPrintf(
      "%s; boundary=\"%s\"", content_type_.c_str(), boundary_.c_str());
}

bool MultiPartFormField::ReadData(void* buffer,
                                  size_t size_to_read,
                                  size_t* size_read,
                                  chromeos::ErrorPtr* error) {
  if (read_stage_ == ReadStage::kStart) {
    if (parts_.empty()) {
      *size_read = 0;
      return true;
    }
    read_stage_ = ReadStage::kBoundarySetup;
    read_part_index_ = 0;
  }

  for (;;) {
    if (read_stage_ == ReadStage::kBoundarySetup &&
        read_part_index_ < parts_.size()) {
      // Starting a new part. Read the part boundary and headers first.
      read_stage_ = ReadStage::kBoundaryData;
      std::string boundary;
      if (read_part_index_ > 0)
        boundary = "\r\n";
      boundary +=
          GetBoundaryStart() + parts_[read_part_index_]->GetContentHeader();
      boundary_reader_.SetData(boundary);
    } else if (read_stage_ == ReadStage::kBoundarySetup &&
               read_part_index_ >= parts_.size()) {
      // Final part has been read. Read the closing boundary.
      read_stage_ = ReadStage::kEnd;
      boundary_reader_.SetData("\r\n" + GetBoundaryEnd());
    } else if (read_stage_ == ReadStage::kBoundaryData ||
               read_stage_ == ReadStage::kEnd) {
      // Reading the boundary data (possibly the closing boundary).
      if (!boundary_reader_.ReadData(buffer, size_to_read, size_read, error))
        return false;
      // Remain in the current stage for as long as there is data in the buffer,
      // or we're in kEnd (since there is no next stage for kEnd).
      if (*size_read > 0 || read_stage_ == ReadStage::kEnd)
        return true;
      read_stage_ = ReadStage::kPart;
    } else if (read_stage_ == ReadStage::kPart) {
      // Reading the actual part data.
      if (!parts_[read_part_index_]->ReadData(
              buffer, size_to_read, size_read, error)) {
        return false;
      }
      if (*size_read > 0)
        return true;
      read_part_index_++;
      read_stage_ = ReadStage::kBoundarySetup;
    }
  }
  return false;
}

void MultiPartFormField::AddCustomField(std::unique_ptr<FormField> field) {
  parts_.push_back(std::move(field));
}

void MultiPartFormField::AddTextField(const std::string& name,
                                      const std::string& data) {
  AddCustomField(std::unique_ptr<FormField>{new TextFormField{name, data}});
}

bool MultiPartFormField::AddFileField(const std::string& name,
                                      const base::FilePath& file_path,
                                      const std::string& content_disposition,
                                      const std::string& content_type,
                                      chromeos::ErrorPtr* error) {
  base::File file{file_path, base::File::FLAG_OPEN | base::File::FLAG_READ};
  if (!file.IsValid()) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    return false;
  }
  std::string file_name = file_path.BaseName().value();
  std::unique_ptr<FormField> file_field{new FileFormField{name,
                                                          file.Pass(),
                                                          file_name,
                                                          content_disposition,
                                                          content_type,
                                                          "binary"}};
  AddCustomField(std::move(file_field));
  return true;
}

std::string MultiPartFormField::GetBoundaryStart() const {
  return base::StringPrintf("--%s\r\n", boundary_.c_str());
}

std::string MultiPartFormField::GetBoundaryEnd() const {
  return base::StringPrintf("--%s--", boundary_.c_str());
}

FormData::FormData() : FormData{std::string{}} {
}

FormData::FormData(const std::string& boundary)
    : form_data_{"", mime::multipart::kFormData, boundary} {
}

void FormData::AddCustomField(std::unique_ptr<FormField> field) {
  form_data_.AddCustomField(std::move(field));
}

void FormData::AddTextField(const std::string& name, const std::string& data) {
  form_data_.AddTextField(name, data);
}

bool FormData::AddFileField(const std::string& name,
                            const base::FilePath& file_path,
                            const std::string& content_type,
                            chromeos::ErrorPtr* error) {
  return form_data_.AddFileField(
      name, file_path, content_disposition::kFormData, content_type, error);
}

std::string FormData::GetContentType() const {
  return form_data_.GetContentType();
}

uint64_t FormData::GetDataSize() const {
  return form_data_.GetDataSize();
}

bool FormData::ReadData(void* buffer,
                        size_t size_to_read,
                        size_t* size_read,
                        chromeos::ErrorPtr* error) {
  return form_data_.ReadData(buffer, size_to_read, size_read, error);
}

}  // namespace http
}  // namespace chromeos
