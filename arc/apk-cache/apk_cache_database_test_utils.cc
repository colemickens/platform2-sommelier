// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/apk-cache/apk_cache_database_test_utils.h"

#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <sqlite3.h>

#include "arc/apk-cache/apk_cache_database.h"

namespace apk_cache {

namespace {

constexpr std::array<const char*, 8> kCreateDatabaseSQL = {
    "PRAGMA foreign_keys = off",
    "CREATE TABLE sessions ("
    "  id         INTEGER PRIMARY KEY AUTOINCREMENT"
    "                     NOT NULL,"
    "  source     TEXT    NOT NULL,"
    "  timestamp  INTEGER NOT NULL,"
    "  attributes TEXT,"
    "  status     INTEGER NOT NULL"
    ")",
    "CREATE TABLE file_entries ("
    "  id           INTEGER PRIMARY KEY AUTOINCREMENT"
    "                       NOT NULL,"
    "  package_name TEXT    NOT NULL,"
    "  version_code INTEGER NOT NULL,"
    "  type         TEXT    NOT NULL,"
    "  attributes   TEXT,"
    "  size         INTEGER NOT NULL,"
    "  hash         TEXT,"
    "  access_time  INTEGER NOT NULL,"
    "  priority     INTEGER NOT NULL,"
    "  session_id   INTEGER NOT NULL,"
    "  FOREIGN KEY ("
    "      session_id"
    "  )"
    "  REFERENCES sessions (id) ON UPDATE NO ACTION"
    "                           ON DELETE CASCADE"
    ")",
    "CREATE INDEX index_hash ON file_entries ("
    "  hash"
    ")",
    "CREATE INDEX index_package_version_type ON file_entries ("
    "  package_name,"
    "  version_code,"
    "  type"
    ")",
    "CREATE INDEX index_session_id ON file_entries ("
    "  session_id"
    ")",
    "CREATE INDEX index_status ON sessions ("
    "  status"
    ")",
    "PRAGMA foreign_keys = on"};

int ExecSQL(const base::FilePath& db_path,
            const std::vector<std::string>& sqls) {
  sqlite3* db;
  int result;
  result = sqlite3_open(db_path.MaybeAsASCII().c_str(), &db);
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_ptr(db, &sqlite3_close);
  if (result != SQLITE_OK)
    return result;

  for (const auto& sql : sqls) {
    result = sqlite3_exec(db_ptr.get(), sql.c_str(), nullptr, nullptr, nullptr);
    if (result != SQLITE_OK)
      return result;
  }

  return sqlite3_close(db_ptr.release());
}

}  // namespace

int CreateDatabase(const base::FilePath& db_path) {
  std::vector<std::string> create_db_sql(kCreateDatabaseSQL.begin(),
                                         kCreateDatabaseSQL.end());
  return ExecSQL(db_path, create_db_sql);
}

void InsertSession(const base::FilePath& db_path, const Session& session) {
  std::ostringstream sql;
  sql << "INSERT INTO sessions (id,source,timestamp,attributes,status) VALUES("
      << session.id << ",'" << session.source << "',"
      << session.timestamp.ToJavaTime() << ",";
  if (session.attributes) {
    sql << "'" << *(session.attributes) << "'";
  } else {
    sql << "null";
  }
  sql << "," << session.status << ")";
  ExecSQL(db_path, {sql.str()});
}

void InsertFileEntry(const base::FilePath& db_path,
                     const FileEntry& file_entry) {
  std::ostringstream sql;
  sql << "INSERT INTO file_entries (id,package_name,version_code,type,"
         "attributes,size,hash,access_time,priority,session_id) VALUES("
      << file_entry.id << ",'" << file_entry.package_name << "',"
      << file_entry.version_code << ",'" << file_entry.type << "',";
  if (file_entry.attributes) {
    sql << "'" << *(file_entry.attributes) << "'";
  } else {
    sql << "null";
  }
  sql << "," << file_entry.size << ",";
  if (file_entry.hash) {
    sql << "'" << *(file_entry.hash) << "'";
  } else {
    sql << "null";
  }
  sql << "," << file_entry.access_time.ToJavaTime() << ","
      << file_entry.priority << "," << file_entry.session_id << ")";
  ExecSQL(db_path, {sql.str()});
}

}  // namespace apk_cache
