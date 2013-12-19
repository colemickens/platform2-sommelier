# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A collection of context managers for working with shill objects."""

import logging
import shill_proxy


class ContextError(Exception):
    """An error raised by a context managers dealing with shill objects."""
    pass


class ServiceAutoConnectContext(object):
    """A context manager for overriding a service's 'AutoConnect' property.

    As the service object of the same service may change during the lifetime
    of the context, this context manager does not take a service object at
    construction. Instead, it takes a |get_service| function at construction,
    which it invokes to obtain a service object when entering and exiting the
    context. It is assumed that |get_service| always returns a service object
    that refers to the same service.

    Usage:
        def get_service():
            # Some function that returns a service object.

        with ServiceAutoConnectContext(get_service, False):
            # Within this context, the 'AutoConnect' property of the service
            # returned by |get_service| is temporarily set to False if it's
            # initially set to True. The property is restored to its initial
            # value after the context ends.

    """
    def __init__(self, get_service, autoconnect):
        self._get_service = get_service
        self._autoconnect = autoconnect
        self._initial_autoconnect = None


    def __enter__(self):
        service = self._get_service()
        if service is None:
            raise ContextError('Could not obtain a service object.')

        # Always set the AutoConnect property even if the requested value
        # is the same so that shill will retain the AutoConnect property, else
        # shill may override it.
        service_properties = service.GetProperties()
        self._initial_autoconnect = shill_proxy.ShillProxy.dbus2primitive(
            service_properties[
                shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT])
        logging.info('ServiceAutoConnectContext: change autoconnect to %s',
                     self._autoconnect)
        service.SetProperty(
            shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT,
            self._autoconnect)

        # Make sure the cellular service gets persisted by taking it out of
        # the ephemeral profile.
        if not service_properties[
                shill_proxy.ShillProxy.SERVICE_PROPERTY_PROFILE]:
            shill = shill_proxy.ShillProxy.get_proxy()
            manager_properties = shill.manager.GetProperties(utf8_strings=True)
            active_profile = manager_props[
                    shill_proxy.ShillProxy.MANAGER_PROPERTY_ACTIVE_PROFILE]
            logging.info('ServiceAutoConnectContext: change cellular service '
                         'profile to %s', active_profile)
            service.SetProperty(
                    shill_proxy.ShillProxy.SERVICE_PROPERTY_PROFILE,
                    active_profile)

        return self


    def __exit__(self, exc_type, exc_value, traceback):
        if self._initial_autoconnect != self._autoconnect:
            service = self._get_service()
            if service is None:
                raise ContextError('Could not obtain a service object.')

            logging.info('ServiceAutoConnectContext: restore autoconnect to %s',
                         self._initial_autoconnect)
            service.SetProperty(
                shill_proxy.ShillProxy.SERVICE_PROPERTY_AUTOCONNECT,
                self._initial_autoconnect)
        return False


    @property
    def autoconnect(self):
        """AutoConnect property value within this context."""
        return self._autoconnect


    @property
    def initial_autoconnect(self):
        """Initial AutoConnect property value when entering this context."""
        return self._initial_autoconnect
