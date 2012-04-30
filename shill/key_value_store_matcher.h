// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_KEY_VALUE_STORE_MATCHER_
#define SHILL_KEY_VALUE_STORE_MATCHER_

#include <gmock/gmock.h>
#include "shill/key_value_store.h"

// A GMock matcher for a shill::KeyValueStore.  To be used in unittests.
//
// Usage:
//   KeyValueStore expected;
//   expected.SetBool(flimflam::kSIMLockEnabledProperty, false);
//   EXPECT_CALL(*device_adaptor_, EmitKeyValueStoreChanged(
//       flimflam::kSIMLockStatusProperty,
//       KeyValueStoreEq(expected)));
//
// The EXPECT_CALL will match if |expected| has the same contents as the actual
// parameter passed to EmitKeyValueStoreChanged().  If the match fails, a helped
// message is emitted to show the mismatch between the actual and expected
// parameters, like this:
//
//   Expected arg #1: KeyValueStore
//      bools-map equals { ("LockEnabled", false) } and
//      int-map equals {} and
//      string-map equals {} and
//      uint-map equals {}
//            Actual: 96-byte object <C607 51F6 0000 0000 089C F0F8 089C F0F8
//            089C F0F8 0100 0000 14A3 3FF7 0000 0000 0000 0000 585C E5FF 585C
//            E5FF 0000 0000 0000 0000 0000 0000 489C F0F8 489C F0F8 489C F0F8
//            0100 0000 AB5C E5FF 0000 0000 889C F0F8 889C F0F8 889C F0F8 0100
//            0000> (
//      Unmatched bools: Only in actual: ("LockEnabled", true); not in actual:
//      ("LockEnabled", false)
//      Unmatched ints:
//      Unmatched strings: Only in actual: ("LockType", "")
//      Unmatched uints: Only in actual: ("RetriesLeft", 0))
//          Expected: to be called once
//            Actual: never called - unsatisfied and active
//
// This shows you the expected value of the KeyValueStore, raw actual
// KeyValueStore (which you can ignore), and, most importantly, the difference
// between the actual and expected values.

namespace shill {

// This class is implemented instead of a simpler MATCHER_P so that the error
// messages are clearer when a match fails.  A KeyValueStore is a aggregate of
// maps.  This matcher uses ContainerEq to compare the maps of the expected
// value to the actual one.  It also uses the the DescribeTo() and
// ExplainMatchResultsTo() methods of ContainerEq to print a useful message when
// the actual and expected values differ.  To do this, it needs a ContainerEq
// object for each map contained by a KeyValueStore.
class KeyValueStoreEqMatcher :
      public testing::MatcherInterface<const KeyValueStore &> {
 public:
  explicit KeyValueStoreEqMatcher(const KeyValueStore &expected)
      : expected_(expected),
        bool_matcher_(testing::ContainerEq(expected_.bool_properties()).impl()),
        int_matcher_(testing::ContainerEq(expected_.int_properties()).impl()),
        string_matcher_(testing::ContainerEq(expected_.string_properties())
                        .impl()),
        uint_matcher_(testing::ContainerEq(expected_.uint_properties()).impl())
  {}
  // Returns true iff the matcher matches |actual|.  Each map of the actual
  // KeyValueStore is compared to the maps contained with the |expected_|
  // KeyValueStore.  The comparison is done by the ContainerEq objects creating
  // in the constructor.
  virtual bool Matches(const KeyValueStore &actual) const {
    return bool_matcher_.impl().Matches(actual.bool_properties()) &&
        int_matcher_.impl().Matches(actual.int_properties()) &&
        string_matcher_.impl().Matches(actual.string_properties()) &&
        uint_matcher_.impl().Matches(actual.uint_properties());
  }
  // Describes this matcher to an ostream.
  virtual void DescribeTo(::std::ostream* os) const {
    *os << "KeyValueStore" << std::endl << "\tbools-map ";
    bool_matcher_.impl().DescribeTo(os);
    *os << " and" << std::endl << "\tint-map ";
    int_matcher_.impl().DescribeTo(os);
    *os << " and" << std::endl << "\tstring-map ";
    string_matcher_.impl().DescribeTo(os);
    *os << " and" << std::endl << "\tuint-map ";
    uint_matcher_.impl().DescribeTo(os);
  }

  // Explains why a match failed.
  virtual void ExplainMatchResultTo(const KeyValueStore &actual,
                                    ::std::ostream* os) const {
    *os << std::endl << "\tUnmatched bools: ";
    bool_matcher_.impl().ExplainMatchResultTo(actual.bool_properties(), os);
    *os << std::endl << "\tUnmatched ints: ";
    int_matcher_.impl().ExplainMatchResultTo(actual.int_properties(), os);
    *os << std::endl << "\tUnmatched strings: ";
    string_matcher_.impl().ExplainMatchResultTo(actual.string_properties(), os);
    *os << std::endl << "\tUnmatched uints: ";
    uint_matcher_.impl().ExplainMatchResultTo(actual.uint_properties(), os);
  }

 private:
  // The expected value.  If |actual| passed to Matches does not equal this
  // value, the match fails.
  const KeyValueStore expected_;

  // These are the ContainerEq objects used in the matching.  This signature is
  // lifted from gmock/gmock-matchers.h.  One of these matchers is required for
  // each map contained within a KeyValueStore.
  testing::PolymorphicMatcher<
    testing::internal::ContainerEqMatcher<std::map<std::string, bool> > >
    bool_matcher_;

  testing::PolymorphicMatcher<
    testing::internal::ContainerEqMatcher<std::map<std::string, int> > >
    int_matcher_;

  testing::PolymorphicMatcher<
    testing::internal::ContainerEqMatcher<std::map<std::string, std::string> > >
    string_matcher_;

  testing::PolymorphicMatcher<
    testing::internal::ContainerEqMatcher<std::map<std::string, uint> > >
    uint_matcher_;
};

inline testing::Matcher<const KeyValueStore&> KeyValueStoreEq(
    const KeyValueStore &expected) {
  return testing::MakeMatcher(new KeyValueStoreEqMatcher(expected));
}

}  // namespace shill
#endif  // SHILL_KEY_VALUE_STORE_MATCHER_
