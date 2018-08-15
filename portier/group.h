// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_GROUP_H_
#define PORTIER_GROUP_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>

namespace portier {

// Groups names should be easy to type/remember group names.  These
// names will likely be typed on a shell.  Group names can contain
// alphanumeric characters or underscores.
bool IsValidGroupName(const std::string& group_name);

// The Group class is used to track all the group members of a
// group.  When adding members to the group, the group will add
// a reference to itself to the added member.  This allows for both the
// member and the group to agree on membership.
//
// The relationship between group members and groups is many-to-one.
//
//  +---------+                            +-----------+
//  |         |-----(Strong Reference)---->|           |
//  |         |                          * |   Group   |
//  |  Group  |                            |   Member  |
//  |         |<-----(Weak Reference)------|(interface)|
//  +---------+                            +-----------+
//                                               A
//                                               |
//                                         +------------+
//                                         |  Concrete  |
//                                         |   Member   |
//                                         +------------+
//
// The template parameter |MemberType| must be a type which implements
// GroupMemberInterface.
template <class MemberType>
class Group {
 public:
  using iterator = typename std::vector<std::shared_ptr<MemberType>>::iterator;

  // Creates a new group, returning a shared pointer to the group.
  // Returns and empty pointer if the group name is invalid.
  static std::unique_ptr<Group> Create(const std::string& name);

  ~Group() { RemoveAllMembers(); }

  // Returns the name of the group.
  std::string name() const { return name_; }

  // Number of members in group.
  size_t size() const { return members_.size(); }

  // Returns a copy of the list of all group members.
  std::vector<std::shared_ptr<MemberType>> GetMembers() const {
    return members_;
  }

  // An iterator for going through all the members.
  iterator begin() { return members_.begin(); }
  iterator end() { return members_.end(); }

  // Adds a new member to the group.
  // If the provided |member| is not a member of any group, then:
  //  1 - The member will be added to the this group,
  //  2 - The member will be given a reference to this group,
  //  3 - The `PostJoinGroup()` hock will be called on the member,
  //  4 - Function will return true.
  //
  // If the provided |member| is already a member of this group, then
  // no action is performed and true is returned.
  //
  // If the provided |member| is already part of a different group,
  // then false is returned.
  bool AddMember(std::shared_ptr<MemberType> member);

  // Removes a specified member from the group.
  // If |member| pointers to a member of the group, then:
  //  1 - The member will be remove from the list of group members,
  //  2 - The member's copy of the groups reference will be reset,
  //  3 - The `PostLeaveGroup()` hook will be called on the member.
  //
  // If the provided |member| if not a member of the group, then
  // no change is made and false is returned.
  bool RemoveMember(std::shared_ptr<MemberType> member);

  // Removes all of the current members from the group.  This will
  // call the `PostLeaveGroup()` hook on all members being removed.
  void RemoveAllMembers();

  // Gets the current upstream interface if set, otherwise returns a
  // nullptr.
  std::shared_ptr<MemberType> GetUpstream() const { return upstream_; }

  // Sets the group's upstream interface.  Specified |member| must be a
  // member of the group or call will fail and return false.
  // This will replace the current upstream if already set.  Returns
  // true even if the current upstream is the same interface.
  bool SetUpstream(std::shared_ptr<MemberType> member);

  // Removes the upstream interface as the upstream interface.  The
  // interface will remain a member of the group.
  void UnsetUpstream() { upstream_.reset(); }

 private:
  // Constructor to the interface is private to enforce the
  // creation of a shared pointer to the interface.
  explicit Group(const std::string& name) : name_(name) {}

  // Removes a member from the group via an interation position to the
  // the internal |members_| vector.  This will remove the group from
  // the member and call the `PostLeaveGroup()` hook.  Returns a copy
  // of the same iterator returned by the call to
  // |members_.erase(position)|.
  // This function is intended to be used by the public methods
  // `RemoveMember()` and `RemoveAllMembers()`.
  iterator Remove(iterator position);

  // Name of the group.
  std::string name_;

  // A list of all members.
  std::vector<std::shared_ptr<MemberType>> members_;

  // A pointer to the upstream interface.
  std::shared_ptr<MemberType> upstream_;

  DISALLOW_COPY_AND_ASSIGN(Group);
};  // class Group

template <class MemberType>
std::unique_ptr<Group<MemberType>> Group<MemberType>::Create(
    const std::string& name) {
  using GroupType = Group<MemberType>;
  if (!IsValidGroupName(name)) {
    return nullptr;
  }
  auto group_ptr = std::unique_ptr<GroupType>(new GroupType(name));
  return std::move(group_ptr);
}

template <class MemberType>
bool Group<MemberType>::AddMember(std::shared_ptr<MemberType> member) {
  DCHECK(member);
  if (member->HasGroup()) {
    // Return true if the member is already a member of the group, and
    // false if the group is different.  Either way, no other action is
    // required.
    return member->GetGroup() == this;
  }
  members_.push_back(member);
  member->SetGroup(this);
  member->PostJoinGroup();
  return true;
}

template <class MemberType>
bool Group<MemberType>::RemoveMember(std::shared_ptr<MemberType> member) {
  DCHECK(member);
  if (!member->HasGroup() || member->GetGroup() != this) {
    return false;
  }
  auto it = std::find(members_.begin(), members_.end(), member);
  // The member must be part of the list, otherwise, there is a bug
  // that has somehow released the member from the group without
  // removing the reference to the group.
  DCHECK(it != members_.end());
  Remove(it);
  return true;
}

template <class MemberType>
void Group<MemberType>::RemoveAllMembers() {
  auto it = members_.begin();
  while (it != members_.end()) {
    it = Remove(it);
  }
}

template <class MemberType>
bool Group<MemberType>::SetUpstream(std::shared_ptr<MemberType> member) {
  DCHECK(member);
  if (!member->HasGroup() || member->GetGroup() != this) {
    return false;
  }
  upstream_ = member;
  return true;
}

// private.
template <class MemberType>
typename Group<MemberType>::iterator Group<MemberType>::Remove(iterator it) {
  DCHECK(it != members_.end());
  std::shared_ptr<MemberType> member = *it;
  if (member == upstream_) {
    upstream_.reset();
  }
  auto next_it = members_.erase(it);
  member->ResetGroup();
  member->PostLeaveGroup();
  return next_it;
}

// Group Member interface.
template <class DerivedType>
class GroupMemberInterface {
 public:
  using GroupType = Group<DerivedType>;

  GroupMemberInterface() : group_(nullptr) {}
  virtual ~GroupMemberInterface() = default;

  // Returns true if the member is a member of a group.
  bool HasGroup() const { return group_ != nullptr; }

  // Returns true if the member is the upstream of the group.
  bool IsUpstream() const {
    return HasGroup() ? GetGroup()->GetUpstream().get() == this : false;
  }

  // Returns a pointer to the group which the member is a part
  // of.  If the member is not part of any group then the pointer
  // returned is null.
  GroupType* GetGroup() const { return group_; }

 protected:
  // Hook methods for classes which inherit from this.  Used to
  // indicate to the subclass that they have been removed from a
  // group.

  // Call after a member has been added to a group.
  virtual void PostJoinGroup() = 0;

  // Called after the member has been removed from a group.
  virtual void PostLeaveGroup() = 0;

 private:
  // Provides a weak reference to the group which the member is a part
  // of.
  void SetGroup(GroupType* group) { group_ = group; }
  void ResetGroup() { group_ = nullptr; }

  // A pointer to the group which the member is a part of. It is
  // expected that the Group class will clear this pointer when
  // removing the member, or when the group's destructor is called.
  GroupType* group_;

  // Used to all the Group class to access the `SetGroup()`
  // and `ResetGroup()` methods as well as the post join/leave
  // group hooks when adding and removing the member from the group.
  friend class Group<DerivedType>;

  DISALLOW_COPY_AND_ASSIGN(GroupMemberInterface);
};  // class GroupMember

}  // namespace portier

#endif  // PORTIER_GROUP_H_
