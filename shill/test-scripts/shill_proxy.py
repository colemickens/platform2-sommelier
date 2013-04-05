# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import logging
import time

def dbus2primitive(value):
    if isinstance(value, dbus.Boolean):
        return bool(value)
    elif isinstance(value, int):
        return int(value)
    elif isinstance(value, float):
        return float(value)
    elif isinstance(value, str):
        return str(value)
    elif isinstance(value, unicode):
        return str(value)
    elif isinstance(value, list):
        return [dbus2primitive(x) for x in value]
    elif isinstance(value, tuple):
        return tuple([dbus2primitive(x) for x in value])
    elif isinstance(value, dict):
        return dict([(dbus2primitive(k), dbus2primitive(v))
                     for k,v in value.items()])
    else:
        logging.error('Failed to convert dbus object of class: %r',
                      value.__class__.__name__)
        return value


class ShillProxy(object):
    DBUS_INTERFACE = 'org.chromium.flimflam'
    DBUS_TYPE_DEVICE = 'org.chromium.flimflam.Device'
    DBUS_TYPE_MANAGER = 'org.chromium.flimflam.Manager'
    DBUS_TYPE_PROFILE = 'org.chromium.flimflam.Profile'
    DBUS_TYPE_SERVICE = 'org.chromium.flimflam.Service'

    MANAGER_PROPERTY_ACTIVE_PROFILE = 'ActiveProfile'
    MANAGER_PROPERTY_DEVICES = 'Devices'
    MANAGER_PROPERTY_PROFILES = 'Profiles'
    MANAGER_PROPERTY_SERVICES = 'Services'

    OBJECT_TYPE_PROPERTY_MAP = {
        'Device': ( self.DBUS_TYPE_DEVICE, self.MANAGER_PROPERTY_DEVICES ),
        'Profile': ( self.DBUS_TYPE_PROFILE, self.MANAGER_PROPERTY_PROFILES ),
        'Service': ( self.DBUS_TYPE_SERVICE, self.MANAGER_PROPERTY_SERVICES )
    }

    SERVICE_DISCONNECT_TIMEOUT = 5

    SERVICE_PROPERTY_GUID = 'GUID'
    SERVICE_PROPERTY_MODE = 'Mode'
    SERVICE_PROPERTY_NAME = 'Name'
    SERVICE_PROPERTY_PASSPHRASE = 'Passphrase'
    SERVICE_PROPERTY_SAVE_CREDENTIALS = 'SaveCredentials'
    SERVICE_PROPERTY_SECURITY = 'Security'
    SERVICE_PROPERTY_SSID = 'SSID'
    SERVICE_PROPERTY_STATE = 'State'
    SERVICE_PROPERTY_TYPE = 'Type'

    POLLING_INTERVAL_SECONDS = 0.2

    UNKNOWN_METHOD = 'org.freedesktop.DBus.Error.UnknownMethod'


    def wait_for_property_in(self, dbus_object, property_name,
                             expected_values, timeout_seconds):
        """
        Block until the property |property_name| in |dbus_object| is in
        |expected_values|, or |timeout_seconds|.

        @param dbus_object DBus proxy object as returned by
            self.get_dbus_object.
        @param property_name string property key in dbus_object.
        @param expected_values iterable set of values to return successfully
            upon seeing.
        @param timeout_seconds float number of seconds to return if we haven't
            seen the appropriate property value in time.
        @return tuple(successful, final_value, duration)
            where successful is True iff we saw one of |expected_values| for
            |property_name|, final_value is the member of |expected_values| we
            saw, and duration is how long we waited to see that value.

        """
        # TODO(wiley) This should be changed to be event based (not polling).
        value = None
        start_time = time.time()
        successful = False
        while time.time() < start_time + timeout_seconds:
            properties = dbus_object.GetProperties(utf8_strings=True)
            value = properties.get(property_name, None)
            if value in expected_values:
                successful = True
                break
            time.sleep(self.POLLING_INTERVAL_SECONDS)
        duration = time.time() - start_time
        return (successful, dbus2primitive(value), duration)


    def __init__(self, bus=None):
        if bus is None:
            bus = dbus.SystemBus()
        self._bus = bus
        self._manager = self.get_dbus_object(self.DBUS_TYPE_MANAGER, '/')


    @property
    def manager(self):
        """ @return DBus proxy object representing the shill Manager. """
        return self._manager


    def get_active_profile(self):
        """
        Get the active profile in shill.

        @return dbus object representing the active profile.

        """
        properties = self.manager.GetProperties(utf8_strings=True)
        return self.get_dbus_object(
                self.DBUS_TYPE_PROFILE,
                properties[self.MANAGER_PROPERTY_ACTIVE_PROFILE])


    def get_dbus_object(self, type_str, path):
        """
        Return the DBus object of type |type_str| at |path| in shill.

        @param type_str string (e.g. self.DBUS_TYPE_SERVICE).
        @param path path to object in shill (e.g. '/service/12').
        @return DBus proxy object.

        """
        return dbus.Interface(
                self._bus.get_object(self.DBUS_INTERFACE, path),
                type_str)


    def get_profiles(self):
        properties = self.manager.GetProperties(utf8_strings=True)
        return [self.get_dbus_object(self.DBUS_TYPE_PROFILE, path)
                for path in properties[self.MANAGER_PROPERTY_PROFILES]]


    def get_service(self, params):
        """
        Get the shill service that matches |params|.

        @param params dict of strings understood by shill to describe
            a service.
        @return DBus object interface representing a service.

        """
        path = self.manager.GetService(params)
        return self.get_dbus_object(self.DBUS_TYPE_SERVICE, path)


    def connect_to_wifi_network(self,
                                ssid,
                                security,
                                psk,
                                save_credentials,
                                discovery_timeout_seconds=15,
                                association_timeout_seconds=15,
                                configuration_timeout_seconds=15):
        """
        Connect to a WiFi network with the given association parameters.

        @param ssid string name of network to connect to.
        @param security string type of security used in network (e.g. psk)
        @param psk string password or pre shared key for appropriate security
                types.
        @param save_credentials bool True if we should save EAP credentials.
        @param discovery_timeout_seconds float timeout for service discovery.
        @param association_timeout_seconds float timeout for service
            association.
        @param configuration_timeout_seconds float timeout for DHCP
            negotiations.
        @return (successful, discovery_time, association_time,
                 configuration_time, reason)
            where successful is True iff the operation succeeded, *_time is
            the time spent waiting for each transition, and reason is a string
            which may contain a meaningful description of failures.

        """
        # TODO(wiley) This doesn't work for hidden ssids
        logging.info('Attempting to conect to %s', ssid)
        service_proxy = None
        start_time = time.time()
        discovery_time = -1.0
        association_time = -1.0
        configuration_time = -1.0
        logging.info('Discovering...')
        while time.time() - start_time < discovery_timeout_seconds:
            discovery_time = time.time() - start_time
            discovery_params = {self.SERVICE_PROPERTY_TYPE: 'wifi',
                                self.SERVICE_PROPERTY_NAME: ssid,
                                self.SERVICE_PROPERTY_SECURITY: security}
            try:
                service_path = self.manager.FindMatchingService(
                        discovery_params)
            except dbus.exceptions.DBusException, e:
                # There really is not a better way to check the error type.
                if e.get_dbus_message() != 'Matching service was not found':
                    logging.error('Caught an error while discovering: %s',
                                  e.get_dbus_message())
                # This is spammy, but shill handles that for us.
                self.manager.RequestScan('wifi')
                time.sleep(self.POLLING_INTERVAL_SECONDS)
                continue
            logging.info('Discovered service: %s', ssid)
            break
        else:
            return (False, discovery_time, association_time,
                    configuration_time, 'FAIL(Discovery timed out)')

        # At this point, we know |service| is in the service list.  Attempt
        # to connect it, and watch the states roll by.
        logging.info('Connecting...')
        try:
            service_object = self.get_dbus_object(self.DBUS_TYPE_SERVICE,
                                                  service_path)
            # Don't give a passphrase unless we actually have one.
            if psk:
                service_object.SetProperty(self.SERVICE_PROPERTY_PASSPHRASE,
                                           psk)
            service_object.Connect()
            logging.info('Called connect on service')
        except dbus.exceptions.DBusException, e:
            logging.error('Caught an error while trying to connect: %s',
                          e.get_dbus_message())
            return (False, discovery_time, association_time,
                    configuration_time, 'FAIL(Failed to call connect)')

        logging.info('Associating...')
        result = self.wait_for_property_in(
                service_object,
                self.SERVICE_PROPERTY_STATE,
                ('configuration', 'ready', 'portal', 'online'),
                association_timeout_seconds)
        (successful, _, association_time) = result
        if not successful:
            return (False, discovery_time, association_time,
                    configuration_time, 'FAIL(Association timed out)')

        logging.info('Associated with service: %s', ssid)

        logging.info('Configuring...')
        result = self.wait_for_property_in(
                service_object,
                self.SERVICE_PROPERTY_STATE,
                ('ready', 'portal', 'online'),
                configuration_timeout_seconds)
        (successful, _, configuration_time) = result
        if not successful:
            return (False, discovery_time, association_time,
                    configuration_time, 'FAIL(Configuration timed out)')

        logging.info('Configured service: %s', ssid)

        # Great success!
        logging.info('Connected to WiFi service.')
        return (True, discovery_time, association_time, configuration_time,
                'SUCCESS(Connection successful)')


    def disconnect_from_wifi_network(self, ssid, timeout=None):
        """Disconnect from the specified WiFi network.

        Method will succeed if it observes the specified network in the idle
        state after calling Disconnect.

        @param ssid string name of network to disconnect.
        @param timeout float number of seconds to wait for idle.
        @return tuple(success, duration, reason) where:
            success is a bool (True on success).
            duration is a float number of seconds the operation took.
            reason is a string containing an informative error on failure.

        """
        if timeout is None:
            timeout = self.SERVICE_DISCONNECT_TIMEOUT
        service_description = {self.SERVICE_PROPERTY_TYPE: 'wifi',
                               self.SERVICE_PROPERTY_NAME: ssid}
        try:
            service_path = self.manager.FindMatchingService(service_description)
        except dbus.exceptions.DBusException, e:
            return (False,
                    0.0,
                    'Failed to disconnect from %s, service not found.' % ssid)

        service = self.get_dbus_object(self.DBUS_TYPE_SERVICE, service_path)
        service.Disconnect()
        result = self.wait_for_property_in(service,
                                           self.SERVICE_PROPERTY_STATE,
                                           ('idle',),
                                           timeout)
        (successful, final_state, duration) = result
        message = 'Success.'
        if not successful:
            message = ('Failed to disconnect from %s, '
                       'timed out in state: %s.' % (ssid, final_state))
        return (successful, duration, message)


    def find_object(self, object_type, properties):
        """
        Get a the first shill object of |object_type| whose properties match
        that of |properties|.

        @param object_type string representing the type of object to be
            returned, eg "Service"
        @param properties dict of strings understood by shill to describe
            a service.
        @return DBus object interface representing a the object found.

        """
        if object_type not in self.OBJECT_TYPE_PROPERTY_MAP:
          return None

        dbus_type, manager_property = self.OBJECT_TYPE_PROPERTY_MAP[object_type]
        manager_properties = self.manager.GetProperties(utf8_strings=True)
        for path in manager_properties[manager_property]:
          test_object = self.get_dbus_object(dbus_type, path)
          object_properties = test_object.GetProperties(utf8_strings=True)
          for name, value in properties.iteritems():
            if (name not in object_properties or
                dbus2primitive(object_properties[name]) != value):
              break
          else:
            return test_object
        return None
