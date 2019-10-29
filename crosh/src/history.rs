// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides history functionality for crosh commands.

use std::collections::VecDeque;
use std::fmt::{self, Display};
use std::fs::File;
use std::io::{self, BufWriter, Read, Seek, SeekFrom, Write};
use std::path::Path;
use std::str::{from_utf8, Utf8Error};

use remain::sorted;

const DEFAULT_MAX_ENTRIES: usize = 100;
const MAX_HISTORY_SIZE: u64 = 4096;

#[sorted]
pub enum Error {
    IO(io::Error),
    UTF8(Utf8Error),
}

impl Display for Error {
    #[remain::check]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::Error::*;

        #[sorted]
        match self {
            IO(err) => write!(f, "IOError: {}", err),
            UTF8(err) => write!(f, "UTF8Error: {}", err),
        }
    }
}

impl From<io::Error> for Error {
    fn from(err: io::Error) -> Self {
        Error::IO(err)
    }
}

impl From<Utf8Error> for Error {
    fn from(err: Utf8Error) -> Self {
        Error::UTF8(err)
    }
}

pub struct History {
    temp: String, // A slot for a command that hasn't been entered, but not issued.
    index: isize, // Track the position in the queue.
    max_entries: usize,
    commands: VecDeque<String>,
}

impl History {
    pub fn new() -> History {
        History::with_max_capacity(DEFAULT_MAX_ENTRIES)
    }

    pub fn load_from_file(&mut self, path: &Path) -> Result<(), Error> {
        let mut file = File::open(path)?;

        // Grab the last 4kb of the file (or all if it is smaller).
        let file_size = file.seek(SeekFrom::End(0))?;
        let data_size = if file_size > MAX_HISTORY_SIZE {
            file.seek(SeekFrom::End(-(MAX_HISTORY_SIZE as i64)))?;
            MAX_HISTORY_SIZE as usize
        } else {
            file.seek(SeekFrom::Start(0))?;
            file_size as usize
        };
        let mut raw_data = vec![0; data_size];
        file.read_exact(&mut raw_data)?;

        // Find a good starting index if the data wasn't taken from the beginning of the file.
        let start: usize = if file_size < MAX_HISTORY_SIZE {
            0
        } else {
            let (i, _) = raw_data
                .iter()
                .enumerate()
                .find(|(_, c)| **c == b'\n')
                .unwrap_or((data_size, &b' '));
            i
        };

        // Convert to UTF-8 and split by lines.
        let temp = from_utf8(&raw_data[start..])?;
        let mut entries: Vec<&str> = temp.lines().filter(|s| !s.is_empty()).collect();

        // Add entries to the back of the history until it is full.
        while self.commands.len() < self.max_entries && !entries.is_empty() {
            self.commands.push_front(entries.pop().unwrap().to_string());
        }
        Ok(())
    }

    pub fn persist_to_file(&self, path: &Path) -> Result<(), io::Error> {
        let mut writer = BufWriter::new(File::create(path)?);
        for cmd in &self.commands {
            writeln!(writer, "{}", &cmd)?;
        }
        Ok(())
    }

    pub fn with_max_capacity(c: usize) -> History {
        History {
            temp: String::new(),
            index: -1,
            max_entries: c,
            commands: VecDeque::new(),
        }
    }

    pub fn new_entry(&mut self, entry: String) {
        self.temp.clear();
        self.index = -1;

        // Do not add entries that begin with a space.
        if entry.starts_with(' ') {
            return;
        }

        // Dedupe sequential commands.
        if Some(&entry) == self.commands.front() {
            return;
        }

        while self.commands.len() >= self.max_entries {
            self.commands.pop_front();
        }
        self.commands.push_back(entry);
    }

    pub fn previous(&mut self, current: &str) -> Option<&str> {
        // Backup the command the user hasn't executed yet.
        if self.index == -1 {
            self.temp = current.to_string();
        }

        if (self.index + 1) as usize == self.commands.len() {
            return None;
        }

        self.index += 1;
        self.get_entry(self.index as usize)
    }

    pub fn next(&mut self) -> Option<&str> {
        // Make sure we aren't already at the bottom.
        if self.index == -1 {
            return None;
        }

        self.index -= 1;

        // If we reached the bottom, restore the command that hasn't been submitted yet.
        if self.index == -1 {
            return Some(&self.temp);
        }

        // This should always have something to return.
        Some(self.get_entry(self.index as usize).unwrap())
    }

    fn get_entry(&self, reverse_index: usize) -> Option<&str> {
        self.commands
            .get(self.commands.len() - reverse_index - 1)
            .map(|s| &**s)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::fs::read_to_string;

    use tempfile::TempDir;

    const TEST_MAX_ENTRIES: usize = 3;

    #[test]
    fn test_history_with_capacity() {
        let history = History::with_max_capacity(TEST_MAX_ENTRIES);
        assert_eq!(history.max_entries, TEST_MAX_ENTRIES);
    }

    #[test]
    fn test_history_new_entry_bounds() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);

        for i in 0..(TEST_MAX_ENTRIES + 1) {
            history.new_entry(format!("entry {}", i));
        }

        assert_eq!(history.commands.front(), Some(&"entry 1".to_string()));
        assert_eq!(
            history.commands.back(),
            Some(&format!("entry {}", TEST_MAX_ENTRIES).to_string())
        );
    }

    #[test]
    fn test_load_from_file_small_success() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        let tempdir = TempDir::new().unwrap();
        let test_file_name = tempdir.path().join("test_file");

        let mut test_file = File::create(test_file_name.as_path()).unwrap();
        test_file
            .write_all(b"entry 1\nentry 2\nentry 3\n\nentry 4")
            .unwrap();
        drop(test_file);

        assert!(history.load_from_file(test_file_name.as_path()).is_ok());
        assert_eq!(history.commands.len(), 3);
        for i in 4..2 {
            assert_eq!(history.previous(""), Some(&**&format!("entry {}", i)));
        }
    }

    #[test]
    fn test_load_from_file_big_success() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        let tempdir = TempDir::new().unwrap();
        let test_file_name = tempdir.path().join("test_file");

        let mut test_file = File::create(test_file_name.as_path()).unwrap();
        // 0x80 is invalid UTF-8 and would trigger an error if it was not skipped correctly.
        test_file.write(&[0x80; MAX_HISTORY_SIZE as usize]).unwrap();
        test_file.write_all(b"entry 0\nentry 1\nentry 2").unwrap();
        drop(test_file);

        assert!(history.load_from_file(test_file_name.as_path()).is_ok());
        assert_eq!(history.commands.len(), 2);
        for i in 2..1 {
            assert_eq!(history.previous(""), Some(&**&format!("entry {}", i)));
        }
    }

    #[test]
    fn test_load_from_file_utf8error() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        let tempdir = TempDir::new().unwrap();
        let test_file_name = tempdir.path().join("test_file");

        let mut test_file = File::create(test_file_name.as_path()).unwrap();
        test_file.write(&[0x80; 1]).unwrap();
        test_file
            .write_all(b"entry 1\nentry 2\nentry 3\n\nentry 4")
            .unwrap();
        drop(test_file);

        if let Err(Error::UTF8(_)) = history.load_from_file(test_file_name.as_path()) {
        } else {
            panic!();
        }
    }

    #[test]
    fn test_load_from_file_ioerror() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        if let Err(Error::IO(_)) = history.load_from_file(Path::new("/dev/null/does_not_exist")) {
        } else {
            panic!();
        }
    }

    #[test]
    fn test_persist_to_file_success() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        let tempdir = TempDir::new().unwrap();
        let test_file_name = tempdir.path().join("test_file");

        for i in 0..TEST_MAX_ENTRIES {
            history.new_entry(format!("entry {}", i));
        }

        assert!(history.persist_to_file(test_file_name.as_path()).is_ok());
        assert_eq!(
            read_to_string(test_file_name.as_path()).unwrap(),
            "entry 0\nentry 1\nentry 2\n"
        );
    }

    #[test]
    fn test_persist_to_file_ioerror() {
        let history = History::with_max_capacity(TEST_MAX_ENTRIES);
        assert!(history
            .persist_to_file(Path::new("/dev/null/does_not_exist"))
            .is_err());
    }

    #[test]
    fn test_history_new_entry_ignore() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);

        history.new_entry(" entry".to_string());
        assert!(history.commands.is_empty());
    }

    #[test]
    fn test_history_new_entry_dedupe() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);
        let entry: &str = "entry";

        history.new_entry(entry.to_string());
        history.new_entry(entry.to_string());
        assert_eq!(history.commands.len(), 1);
    }

    #[test]
    fn test_history_new_entry_index_reset() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);

        let first_entry = "entry 0";
        history.new_entry(first_entry.to_string());
        assert_eq!(history.previous(""), Some(first_entry));

        let second_entry = "entry 1";
        history.new_entry(second_entry.to_string());
        assert_eq!(history.previous(""), Some(second_entry));
    }

    #[test]
    fn test_history_previous_bounds() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);

        let command = "temp";

        for i in 0..TEST_MAX_ENTRIES {
            history.new_entry(format!("entry {}", i));
        }

        for i in 0..TEST_MAX_ENTRIES {
            assert_eq!(
                history.previous(command),
                Some(&**&format!("entry {}", TEST_MAX_ENTRIES - 1 - i))
            );
        }
        assert_eq!(history.previous(command), None);
    }

    #[test]
    fn test_history_next_bounds() {
        let mut history = History::with_max_capacity(TEST_MAX_ENTRIES);

        for i in 0..TEST_MAX_ENTRIES {
            history.new_entry(format!("entry {}", i));
        }

        let command = "temp";
        history.previous(command);

        // This is one greater than possible, but it simplifies the test.
        history.index = history.commands.len() as isize;

        for i in 0..TEST_MAX_ENTRIES {
            assert_eq!(history.next(), Some(&**&format!("entry {}", i)));
        }
        assert_eq!(history.next(), Some(command));
        assert_eq!(history.next(), None);
    }
}
