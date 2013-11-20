# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import logging
import time

class ShillProxyError(Exception):
    """Exceptions raised by ShillProxy and it's children."""
    pass


class ShillProxy(object):
    # Core DBus error names
    DBUS_ERROR_UNKNOWN_OBJECT = 'org.freedesktop.DBus.Error.UnknownObject'
    # Shill error names
    ERROR_FAILURE = 'org.chromium.flimflam.Error.Failure'

    DBUS_INTERFACE = 'org.chromium.flimflam'
    DBUS_SERVICE_UNKNOWN = 'org.freedesktop.DBus.Error.ServiceUnknown'
    DBUS_TYPE_DEVICE = 'org.chromium.flimflam.Device'
    DBUS_TYPE_MANAGER = 'org.chromium.flimflam.Manager'
    DBUS_TYPE_PROFILE = 'org.chromium.flimflam.Profile'
    DBUS_TYPE_SERVICE = 'org.chromium.flimflam.Service'

    ENTRY_FIELD_NAME = 'Name'

    MANAGER_PROPERTY_ACTIVE_PROFILE = 'ActiveProfile'
    MANAGER_PROPERTY_DEVICES = 'Devices'
    MANAGER_PROPERTY_PROFILES = 'Profiles'
    MANAGER_PROPERTY_SERVICES = 'Services'
    MANAGER_PROPERTY_ALL_SERVICES = 'ServiceCompleteList'

    PROFILE_PROPERTY_ENTRIES = 'Entries'
    PROFILE_PROPERTY_NAME = 'Name'

    OBJECT_TYPE_PROPERTY_MAP = {
        'Device': ( DBUS_TYPE_DEVICE, MANAGER_PROPERTY_DEVICES ),
        'Profile': ( DBUS_TYPE_PROFILE, MANAGER_PROPERTY_PROFILES ),
        'Service': ( DBUS_TYPE_SERVICE, MANAGER_PROPERTY_SERVICES ),
        'AnyService': ( DBUS_TYPE_SERVICE, MANAGER_PROPERTY_ALL_SERVICES )
    }

    SERVICE_DISCONNECT_TIMEOUT = 5

    SERVICE_PROPERTY_AUTOCONNECT = 'AutoConnect'
    SERVICE_PROPERTY_DEVICE = 'Device'
    SERVICE_PROPERTY_GUID = 'GUID'
    SERVICE_PROPERTY_HIDDEN = 'WiFi.HiddenSSID'
    SERVICE_PROPERTY_MODE = 'Mode'
    SERVICE_PROPERTY_NAME = 'Name'
    SERVICE_PROPERTY_PASSPHRASE = 'Passphrase'
    SERVICE_PROPERTY_SAVE_CREDENTIALS = 'SaveCredentials'
    SERVICE_PROPERTY_SECURITY = 'Security'
    SERVICE_PROPERTY_SSID = 'SSID'
    SERVICE_PROPERTY_STRENGTH = 'Strength'
    SERVICE_PROPERTY_STATE = 'State'
    SERVICE_PROPERTY_TYPE = 'Type'

    SUPPORTED_WIFI_STATION_TYPES = {'managed': 'managed',
                                    'ibss': 'adhoc',
                                    None: 'managed'}

    DEVICE_PROPERTY_ADDRESS = 'Address'
    DEVICE_PROPERTY_EAP_AUTHENTICATION_COMPLETED = 'EapAuthenticationCompleted'
    DEVICE_PROPERTY_EAP_AUTHENTICATOR_DETECTED = 'EapAuthenticatorDetected'
    DEVICE_PROPERTY_IP_CONFIG = 'IpConfig'
    DEVICE_PROPERTY_INTERFACE = 'Interface'
    DEVICE_PROPERTY_NAME = 'Name'
    DEVICE_PROPERTY_POWERED = 'Powered'
    DEVICE_PROPERTY_RECEIVE_BYTE_COUNT = 'ReceiveByteCount'
    DEVICE_PROPERTY_TRANSMIT_BYTE_COUNT = 'TransmitByteCount'
    DEVICE_PROPERTY_TYPE = 'Type'

    TECHNOLOGY_CELLULAR = 'cellular'
    TECHNOLOGY_ETHERNET = 'ethernet'
    TECHNOLOGY_VPN = 'vpn'
    TECHNOLOGY_WIFI = 'wifi'
    TECHNOLOGY_WIMAX = 'wimax'

    VALUE_POWERED_ON = 'on'
    VALUE_POWERED_OFF = 'off'

    POLLING_INTERVAL_SECONDS = 0.2

    # Default log level used in connectivity tests.
    LOG_LEVEL_FOR_TEST = -4

    # Default log scopes used in connectivity tests.
    LOG_SCOPES_FOR_TEST_COMMON = [
        'connection',
        'dbus',
        'device',
        'link',
        'manager',
        'portal',
        'service'
    ]

    # Default log scopes used in connectivity tests for specific technologies.
    LOG_SCOPES_FOR_TEST = {
        TECHNOLOGY_CELLULAR: LOG_SCOPES_FOR_TEST_COMMON + ['cellular'],
        TECHNOLOGY_ETHERNET: LOG_SCOPES_FOR_TEST_COMMON + ['ethernet'],
        TECHNOLOGY_VPN: LOG_SCOPES_FOR_TEST_COMMON + ['vpn'],
        TECHNOLOGY_WIFI: LOG_SCOPES_FOR_TEST_COMMON + ['wifi'],
        TECHNOLOGY_WIMAX: LOG_SCOPES_FOR_TEST_COMMON + ['wimax']
    }

    UNKNOWN_METHOD = 'org.freedesktop.DBus.Error.UnknownMethod'


    @staticmethod
    def str2dbus(dbus_class, value):
        """Typecast string property values to dbus types"""
        if isinstance(dbus_class, dbus.Boolean):
            return dbus_class(value.lower() in ('true','1'))
        else:
            return dbus_class(value)


    @classmethod
    def dbus2primitive(cls, value):
        """Typecast values from dbus types to python types."""
        if isinstance(value, dbus.Boolean):
            return bool(value)
        elif isinstance(value, int):
            return int(value)
        elif isinstance(value, dbus.UInt16):
            return long(value)
        elif isinstance(value, dbus.UInt32):
            return long(value)
        elif isinstance(value, dbus.UInt64):
            return long(value)
        elif isinstance(value, float):
            return float(value)
        elif isinstance(value, str):
            return str(value)
        elif isinstance(value, unicode):
            return str(value)
        elif isinstance(value, list):
            return [cls.dbus2primitive(x) for x in value]
        elif isinstance(value, tuple):
            return tuple([cls.dbus2primitive(x) for x in value])
        elif isinstance(value, dict):
            return dict([(cls.dbus2primitive(k), cls.dbus2primitive(v))
                         for k,v in value.items()])
        else:
            logging.error('Failed to convert dbus object of class: %r',
                          value.__class__.__name__)
            return value


    @staticmethod
    def get_dbus_property(interface, property_key):
        """get property on a dbus Interface

        @param interface dbus Interface to receive new setting
        @param property_key string name of property on interface
        @return python typed object representing property value or None

        """
        properties = interface.GetProperties(utf8_strings=True)
        if property_key in properties:
            return ShillProxy.dbus2primitive(properties[property_key])
        else:
            return None


    @staticmethod
    def set_dbus_property(interface, property_key, value):
        """set property on a dbus Interface

        @param interface dbus Interface to receive new setting
        @param property_key string name of property on interface
        @param value string value to set for property on interface from string

        """
        properties = interface.GetProperties(utf8_strings=True)
        if property_key not in properties:
            raise ShillProxyError('No property %s found in %s' %
                    (property_key, interface.object_path))
        else:
            dbus_class = properties[property_key].__class__
            interface.SetProperty(property_key,
                    ShillProxy.str2dbus(dbus_class, value))


    @classmethod
    def get_proxy(cls, timeout_seconds=10):
        """Create a Proxy, retrying if necessary.

        This method creates a proxy object of the required subclass of
        ShillProxy. A call to SomeSubclassOfShillProxy.get_proxy() will return
        an object of type SomeSubclassOfShillProxy.

        Connects to shill over D-Bus. If shill is not yet running,
        retry until it is, or until |timeout_seconds| expires.

        After connecting to shill, this method will verify that shill
        is answering RPCs. No timeout is applied to the test RPC, so
        this method _may_ block indefinitely.

        @param timeout_seconds float number of seconds to try connecting
            A value <= 0 will cause the method to return immediately,
            without trying to connect.
        @return a ShillProxy instance if we connected, or None otherwise

        """
        end_time = time.time() + timeout_seconds
        connection = None
        while connection is None and time.time() < end_time:
            try:
                # We create instance of class on which this classmethod was
                # called. This way, calling SubclassOfShillProxy.get_proxy()
                # will get a proxy of the right type.
                connection = cls()
            except dbus.exceptions.DBusException as e:
                if e.get_dbus_name() != ShillProxy.DBUS_SERVICE_UNKNOWN:
                    raise ShillProxyError('Error connecting to shill')
                else:
                    # Wait a moment before retrying
                    time.sleep(ShillProxy.POLLING_INTERVAL_SECONDS)

        if connection is None:
            return None

        # Although shill is connected to D-Bus at this point, it may
        # not have completed initialization just yet. Call into shill,
        # and wait for the response, to make sure that it is truly up
        # and running. (Shill will not service D-Bus requests until
        # initialization is complete.)
        connection.get_profiles()
        return connection


    def __init__(self, bus=None):
        if bus is None:
            bus = dbus.SystemBus()
        self._bus = bus
        self._manager = self.get_dbus_object(self.DBUS_TYPE_MANAGER, '/')


    def configure_service_by_guid(self, guid, properties={}):
        """Configure a service identified by its GUID.

        @param guid string unique identifier of service.
        @param properties dictionary of service property:value pairs.

        """
        config = properties.copy()
        config[self.SERVICE_PROPERTY_GUID] = guid
        self.manager.ConfigureService(config)


    def set_logging(self, level, scopes):
        """Set the logging in shill to the specified |level| and |scopes|.

        @param level int log level to set to in shill.
        @param scopes list of strings of log scopes to set to in shill.

        """
        self.manager.SetDebugLevel(level)
        self.manager.SetDebugTags('+'.join(scopes))


    def set_logging_for_test(self, technology):
        """Set the logging in shill for a test of the specified |technology|.

        Set the log level to |LOG_LEVEL_FOR_TEST| and the log scopes to the
        ones defined in |LOG_SCOPES_FOR_TEST| for |technology|. If |technology|
        is not found in |LOG_SCOPES_FOR_TEST|, the log scopes are set to
        |LOG_SCOPES_FOR_TEST_COMMON|.

        @param technology string representing the technology type of a test
            that the logging in shill is to be customized for.

        """
        scopes = self.LOG_SCOPES_FOR_TEST.get(technology,
                                              self.LOG_SCOPES_FOR_TEST_COMMON)
        self.set_logging(self.LOG_LEVEL_FOR_TEST, scopes)


    def wait_for_property_in(self, dbus_object, property_name,
                             expected_values, timeout_seconds):
        """Wait till a property is in a list of expected values.

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
            try:
                properties = dbus_object.GetProperties(utf8_strings=True)
            except dbus.exceptions.DBusException:
                value = '(object reference became invalid)'
                break
            value = properties.get(property_name, None)
            if value in expected_values:
                successful = True
                break
            time.sleep(self.POLLING_INTERVAL_SECONDS)
        duration = time.time() - start_time
        return successful, self.dbus2primitive(value), duration


    @property
    def manager(self):
        """ @return DBus proxy object representing the shill Manager. """
        return self._manager


    def get_active_profile(self):
        """Get the active profile in shill.

        @return dbus object representing the active profile.

        """
        properties = self.manager.GetProperties(utf8_strings=True)
        return self.get_dbus_object(
                self.DBUS_TYPE_PROFILE,
                properties[self.MANAGER_PROPERTY_ACTIVE_PROFILE])


    def get_dbus_object(self, type_str, path):
        """Return the DBus object of type |type_str| at |path| in shill.

        @param type_str string (e.g. self.DBUS_TYPE_SERVICE).
        @param path path to object in shill (e.g. '/service/12').
        @return DBus proxy object.

        """
        return dbus.Interface(
                self._bus.get_object(self.DBUS_INTERFACE, path),
                type_str)


    def get_devices(self):
        """Return the list of devices as dbus Interface objects"""
        properties = self.manager.GetProperties(utf8_strings=True)
        return [self.get_dbus_object(self.DBUS_TYPE_DEVICE, path)
                for path in properties[self.MANAGER_PROPERTY_DEVICES]]


    def get_profiles(self):
        """Return the list of profiles as dbus Interface objects"""
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


    def get_service_for_device(self, device):
        """Attempt to find a service that manages |device|.

        @param device a dbus object interface representing a device.
        @return Dbus object interface representing a service if found. None
                otherwise.

        """
        properties = self.manager.GetProperties(utf8_strings=True)
        all_services = properties.get(self.MANAGER_PROPERTY_ALL_SERVICES,
                                      None)
        if not all_services:
            return None

        for service_path in all_services:
            service = self.get_dbus_object(self.DBUS_TYPE_SERVICE,
                                           service_path)
            properties = service.GetProperties(utf8_strings=True)
            device_path = properties.get(self.SERVICE_PROPERTY_DEVICE, None)
            if device_path == device.object_path:
                return service

        return None


    def find_object(self, object_type, properties):
        """Find a shill object with the specified type and properties.

        Return the first shill object of |object_type| whose properties match
        all that of |properties|.

        @param object_type string representing the type of object to be
            returned. Valid values are those object types defined in
            |OBJECT_TYPE_PROPERTY_MAP|.
        @param properties dict of strings understood by shill to describe
            a service.
        @return DBus object interface representing the object found or None
            if no matching object is found.

        """
        if object_type not in self.OBJECT_TYPE_PROPERTY_MAP:
            return None

        dbus_type, manager_property = self.OBJECT_TYPE_PROPERTY_MAP[object_type]
        manager_properties = self.manager.GetProperties(utf8_strings=True)
        for path in manager_properties[manager_property]:
            try:
                test_object = self.get_dbus_object(dbus_type, path)
                object_properties = test_object.GetProperties(utf8_strings=True)
                for name, value in properties.iteritems():
                    if (name not in object_properties or
                        self.dbus2primitive(object_properties[name]) != value):
                        break
                else:
                    return test_object

            except dbus.exceptions.DBusException, e:
                # This could happen if for instance, you're enumerating services
                # and test_object was removed in shill between the call to get
                # the manager properties and the call to get the service
                # properties.  This causes failed method invocations.
                continue
        return None


    def find_matching_service(self, properties):
        """Find a service object that matches the given properties.

        This re-implements the manager DBus method FindMatchingService.
        The advantage of doing this here is that FindMatchingServices does
        not exist on older images, which will cause tests to fail.

        @param properties dict of strings understood by shill to describe
            a service.

        """
        return self.find_object('Service', properties)
