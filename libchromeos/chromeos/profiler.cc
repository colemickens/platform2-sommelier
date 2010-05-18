// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/profiler.h"

#include <sys/time.h>
#include <stdio.h>

#include <cstring>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"

namespace chromeos {

using base::TimeTicks;
using file_util::CloseFile;
using file_util::OpenFile;

//
// Static constants and functions
//
static inline int64 Now() {
  return TimeTicks::Now().ToInternalValue();
}

//
// Marker definition
//
Marker::Marker(Profiler* profiler, const char* name)
    : profiler_(profiler),
      symbol_id_(0) {
  symbol_id_ = profiler->AddSymbol(name);
}

void Marker::Tap() {
  profiler_->AddSample(symbol_id_, Now(), Profiler::MARK_FLAG_TAP);
}

void Marker::Begin() {
  profiler_->AddSample(symbol_id_, Now(), Profiler::MARK_FLAG_BEGIN);
}

void Marker::End() {
  profiler_->AddSample(symbol_id_, Now(), Profiler::MARK_FLAG_END);
}

//
// Profiler definition
//
Profiler::Profiler()
    : profiler_writer_(NULL),
      max_num_symbols_(0),
      max_num_samples_(0),
      num_symbols_(0),
      num_samples_(0),
      symbols_(NULL),
      samples_(NULL) {
}

void Profiler::Start(ProfilerWriter* profiler_writer,
                     unsigned int max_num_symbols,
                     unsigned int max_num_samples) {
  if (IsStarted()) {
    LOG(WARNING) << "the profiler has already started";
  } else if (profiler_writer == NULL) {
    LOG(WARNING) << "profiler writer cannot be NULL";
  } else if (max_num_symbols == 0 || max_num_samples == 0) {
    LOG(WARNING) << "the maximum # of symbols and samples must > 0";
  } else {
    profiler_writer_ = profiler_writer;
    max_num_symbols_ = max_num_symbols;
    max_num_samples_ = max_num_samples;

    symbols_.reset(new Symbol[max_num_symbols_]);
    samples_.reset(new Sample[max_num_samples_]);

    memset(symbols_.get(), 0, sizeof(symbols_[0]) * max_num_symbols_);
    memset(samples_.get(), 0, sizeof(samples_[0]) * max_num_samples_);
  }
}

void Profiler::Stop() {
  if (!IsStarted()) {
    LOG(WARNING) << "the profiler was not started";
    return;
  }
  profiler_writer_->Update(*this);
  max_num_symbols_ = 0;
  max_num_samples_ = 0;
  symbols_.reset(NULL);
  samples_.reset(NULL);
}

unsigned int Profiler::AddSymbol(const char* name) {
  if (!IsStarted() || num_symbols_ == max_num_symbols_) {
    return max_num_symbols_;
  }
  strncpy(symbols_[num_symbols_].name, name,
          sizeof(symbols_[num_symbols_].name - 1));

  return num_symbols_++;
}

void Profiler::AddSample(unsigned int symbol_id, int64 time,
                         Profiler::MarkFlag flag) {
  if (symbol_id >= num_symbols_) {
    LOG(WARNING) << "symbol id provided exceeds number of symbols";
    return;
  }
  samples_[num_samples_].symbol_id = symbol_id;
  samples_[num_samples_].flag = flag;
  samples_[num_samples_].time = time;
  if (++num_samples_ == max_num_samples_) {
    profiler_writer_->Update(*this);
    num_samples_ = 0;
  }
}

//
// ProfilerWriter definition
//
ProfilerWriter::ProfilerWriter(FilePath file_path)
    : num_written_samples_(0),
      num_written_symbols_(0),
      file_path_(file_path) {
}

void ProfilerWriter::Update(const Profiler& profiler) {
  FILE* fp = NULL;
  if (num_written_samples_ == 0) {
    fp = OpenFile(file_path_, "wb");
  } else {
    fp = OpenFile(file_path_, "r+b");
  }

  if (fp == NULL) {
    LOG(WARNING) << "cannot open profile for writing";
    return;
  }

  num_written_samples_ += profiler.num_samples_;

  // overwrite header
  size_t result = 0;
  result = fwrite(&profiler.max_num_symbols_,
                  sizeof(profiler.max_num_symbols_), 1, fp);

  DCHECK_EQ(result, 1);
  result = fwrite(&profiler.num_symbols_,
                  sizeof(profiler.num_symbols_), 1, fp);
  DCHECK_EQ(result, 1);
  result = fwrite(&num_written_samples_, sizeof(num_written_samples_), 1, fp);
  DCHECK_EQ(result, 1);

  if (num_written_symbols_ != profiler.num_symbols_) {
    // overwrite symbols
    result = fwrite(profiler.symbols_.get(), sizeof(profiler.symbols_),
                    profiler.max_num_symbols_, fp);
    DCHECK_EQ(result, profiler.max_num_symbols_);
    num_written_symbols_ = profiler.num_symbols_;
  }

  // append samples
  fseek(fp, 0, SEEK_END);
  result = fwrite(profiler.samples_.get(), sizeof(profiler.samples_),
                  profiler.num_samples_, fp);
  DCHECK_EQ(result, profiler.num_samples_);

  CloseFile(fp);
}

}  // namespace chromeos
