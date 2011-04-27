// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EVENT_
#define SHILL_EVENT_

#include <vector>

namespace shill {

// This is a pure-virtual base class for callback objects which can be
// queued up and called later.  The callback virtual method takes a single
// argument, which will be handed to the dispatcher to call on all listeners.
template <typename Arg>
class Callback {
 public:
  virtual ~Callback() {}
  virtual void Run(Arg arg) = 0;
};

// This is a callback subclass that contains an object and method to call.
// These methods take a passed-in argument specific to the callback type.
template <typename Class, typename Arg>
class ClassCallback : public Callback<Arg> {
 public:
  typedef void (Class::*MethodType)(Arg arg);

  ClassCallback(Class* object, MethodType method)
    : object_(object), method_(method) {}
  ~ClassCallback() {}

  void Run(Arg arg) {
    (object_->*method_)(arg);
  }

 private:
  Class* object_;
  MethodType method_;
};

// This is the event queue superclass, which contains a function for
// dispatching all events in the queue to their respective listeners.
// A common "AlertDispatcher()" function is used by subclasses to alert
// the central dispatcher that events have been queued and a dispatch
// should be performed soon.
class EventDispatcher;
class EventQueueItem {
 public:
  EventQueueItem(EventDispatcher *dispatcher);
  ~EventQueueItem();
  virtual void Dispatch() = 0;
  void AlertDispatcher();
 private:
  EventDispatcher *dispatcher_;
};

// This is a template subclass of EventQueueItem which is specific to
// a particular argument type.  This object contains a queue of events
// waiting for delivery to liesteners, and a list of even callbacks --
// the listeners for this event.
template <typename Arg>
class EventQueue : public EventQueueItem {
  typedef Callback<Arg>CallbackType;

 public:
  explicit EventQueue(EventDispatcher *dispatcher)
    : EventQueueItem(dispatcher) {}

  inline void AddCallback(CallbackType *cb) {
    callback_list_.push_back(cb);
  }

  void RemoveCallback(CallbackType *cb) {
    for (size_t event_idx = 0; event_idx < callback_list_.size(); ++event_idx) {
      if (callback_list_[event_idx] == cb) {
        callback_list_.erase(callback_list_.begin() + event_idx);
        return;
      }
    }
  }

  void Dispatch() {
    for (size_t event_idx = 0; event_idx < event_queue_.size(); ++event_idx)
      for (size_t call_idx = 0; call_idx < callback_list_.size(); ++call_idx)
        callback_list_[call_idx]->Run(event_queue_[event_idx]);
    event_queue_.clear();
  }

  void AddEvent(Arg arg) {
    event_queue_.push_back(arg);
    AlertDispatcher();
  }

 private:
  std::vector<CallbackType *> callback_list_;
  std::vector<Arg> event_queue_;
};

// This is the main event dispatcher.  It contains a central instance, and
// is the entity responsible for dispatching events out of all queues to
// their listeners during the idle loop.
class EventDispatcher {
 public:
  void DispatchEvents();
  void ExecuteOnIdle();
  void RegisterCallbackQueue(EventQueueItem *queue);
  void UnregisterCallbackQueue(EventQueueItem *queue);
 private:
  std::vector<EventQueueItem*> queue_list_;
};

}  // namespace shill

#endif  // SHILL_EVENT_
