// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CORE_COLLECTOR_COREDUMP_WRITER_H_
#define CRASH_REPORTER_CORE_COLLECTOR_COREDUMP_WRITER_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <elf.h>
#include <link.h>

#include <base/containers/hash_tables.h>  // For std::hash on std::pair.
#include <base/files/file_path.h>
#include <base/macros.h>
#include <brillo/streams/stream.h>

// Reads a core dump from an input stream, writes a stripped version thereof to
// disk, and generates files needed for minidump conversion.
class CoredumpWriter {
 public:
  // Core dump is read from |fd|, and written to |coredump_path|. Files needed
  // for minidump conversion are stored in |container_dir|.
  CoredumpWriter(int fd, const char *coredump_path, const char *container_dir);

  // Returns sysexits.h exit code.
  int WriteCoredump();

 private:
  using Ehdr = ElfW(Ehdr);
  using Phdr = ElfW(Phdr);

  // Virtual address range occupied by a mapped file.
  using FileRange = std::pair<ElfW(Addr), ElfW(Addr)>;
  struct FileInfo {
    ElfW(Off) offset;
    std::string path;
  };
  using FileMappings = std::unordered_map<
      FileRange, FileInfo, BASE_HASH_NAMESPACE::hash<FileRange>>;

  class Reader;

  // Reads ELF header, all program headers, and PT_NOTE segment.
  int ReadUntilNote(Reader *reader,
                    Ehdr *elf_header,
                    std::vector<Phdr> *program_headers,
                    std::vector<char> *note_buf);

  // Extracts address ranges occupied by mapped files from PT_NOTE segment.
  static bool GetFileMappings(const std::vector<char> &note_buf,
                              FileMappings *file_mappings);

  // Strips unnecessary segments by setting their size to zero.
  static void StripSegments(const std::vector<Phdr> &program_headers,
                            const FileMappings &file_mappings,
                            std::vector<Phdr> *stripped_program_headers);

  // Writes file in |container_dir_| in the format of /proc/[pid]/auxv.
  int WriteAuxv(const std::vector<char> &note_buf);

  // Writes file in |container_dir_| in the format of /proc/[pid]/maps.
  int WriteMaps(const std::vector<Phdr> &program_headers,
                const FileMappings &file_mappings);

  const brillo::StreamPtr src_;
  brillo::ErrorPtr error_;

  const base::FilePath coredump_path_;
  const base::FilePath container_dir_;

  DISALLOW_COPY_AND_ASSIGN(CoredumpWriter);
};

#endif  // CRASH_REPORTER_CORE_COLLECTOR_COREDUMP_WRITER_H_
