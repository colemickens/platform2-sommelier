# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import dbus.glib
import dbus.mainloop.glib
import gobject
import threading


class DBusMainLoopStillRunningException(Exception):
    """Exception raised when we fail to stop our DBus event loop."""


class SignalReceiverContext(object):
    def __init__(self, threaded_bus, receiver, signal_name=None,
                 dbus_interface=None, object_path=None, path_keyword=None):
        """
        Construct a context in which a DBus signal receiver is active.

        @param threaded_bus: ThreadedDBusBus object.
        @param receiver: method to receive signals.
        @param dbus_interface: string name of DBus interface to listen to.
        @param object_path: string path of object to listen to signals from.
        @param path_keyword: string name of parameter in receiver to accept
                path of signalling DBus object.

        """
        self._threaded_bus = threaded_bus
        self._receiver = receiver
        self._signal_name = signal_name
        self._dbus_interface = dbus_interface
        self._object_path = object_path
        self._path_keyword = path_keyword


    def __enter__(self):
        """
        Attaches a handler to the threaded bus for the specified signal.

        @return self

        """
        self._handler_ref = self._threaded_bus.bus.add_signal_receiver(
                self._receiver,
                signal_name=self._signal_name,
                dbus_interface=self._dbus_interface,
                path=self._object_path,
                # This lets the bus know that we'd like the path of the
                # signalling object, and that we'd like it passed in our
                # |object_path| kwarg.
                path_keyword=self._path_keyword)
        return self


    def __exit__(self, exc_type, exc_value, traceback):
        """
        Removes the signal receiver.

        @param exc_type: unused.
        @param exc_value: unused.
        @param traceback: unused.

        """
        self._handler_ref.remove()


class ThreadedDBusBus(object):
    """
    Wrapper for a DBus proxy that donates a thread for signal processing.

    Usage:

    update_queue = Queue.Queue()
    signal_receiver = _get_update_handler(update_queue)
    threaded_bus = threaded_dbus_bus.ThreadedDBusBus()
    receiver_context = threaded_dbus_bus.SignalReceiverContext(
            threaded_bus, signal_receiver,
            signal_name='PropertyChanged',
            dbus_interface=dbus_object.dbus_interface,
            object_path=dbus_object.object_path,
            path_keyword='object_path')
    with threaded_bus, signal_receiver:
        # Poll the queue for events you're interested in.

    Note that the signal receiver will be called from the thread context
    of the ThreadedDBusBus, and so should only perform thread safe operations.
    For instance, you might enqueue tuples representing DBus signals into a
    thread safe queue, then consume that queue from the main thread.

    """

    def __init__(self):
        # DBus would like us to donate a thread to run a main loop.  Signals
        # don't flow without a running main loop.
        gobject.threads_init()
        dbus.glib.init_threads()
        self._bus = dbus.SystemBus(mainloop=dbus.mainloop.glib.DBusGMainLoop())
        self._loop_runner = None


    def __enter__(self):
        """
        Starts the signal handling thread.

        @return self

        """
        if self._loop_runner is not None:
            logging.error('Tried to use ThreadedDBusBus more than once!')
            return None

        self._loop = gobject.MainLoop()
        self._loop_runner = threading.Thread(target=self._loop.run)
        self._loop_runner.start()
        return self


    def __exit__(self, exc_type, exc_value, traceback):
        """
        Kills the signal handling thread.

        @param exc_type: unused.
        @param exc_value: unused.
        @param traceback: unused.

        """
        self._loop.quit()
        self._loop_runner.join(timeout=5.0)
        if self._loop_runner.isAlive():
            raise DBusMainLoopStillRunningException()


    @property
    def bus(self):
        """@return DBus bus object."""
        return self._bus
