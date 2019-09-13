// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides history functionality for crosh commands.

use std::collections::VecDeque;

const DEFAULT_MAX_ENTRIES: usize = 100;

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
