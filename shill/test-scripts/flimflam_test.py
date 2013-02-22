#!/usr/bin/env python
# Simulates an implementation of ModemManager to test the flimflam modemmanager
# plugin. Test cases should probably subclass Modem.

import datetime
import gobject
import time

import dbus
import dbus.mainloop.glib
import dbus.service
from dbus.exceptions import DBusException
from dbus.types import UInt32
import glib

OFDP = 'org.freedesktop.DBus.Properties'
MM = 'org.freedesktop.ModemManager'
CMM = 'org.chromium.ModemManager'
OCMM = '/org/chromium/ModemManager'

# Modem States
STATE_UNKNOWN = 0
STATE_DISABLED = 10
STATE_DISABLING = 20
STATE_ENABLING = 30
STATE_ENABLED = 40
STATE_SEARCHING = 50
STATE_REGISTERED = 60
STATE_DISCONNECTING = 70
STATE_CONNECTING = 80
STATE_CONNECTED = 90

# State Change Reasons
REASON_UNKNOWN = 0
REASON_USER_REQUESTED = 1
REASON_SUSPEND = 2

# Miscellaneous delays to simulate a *fast* modem
ACTIVATION_DELAY_MS = 500
DISCARD_MODEM_DELAY_MS = 1000
REBOOT_DELAY_MS = 2000
DEFAULT_CONNECT_DELAY_MS = 1500

# List of GSM Registration Status
GSM_REG_STATUS_IDLE = 0
GSM_REG_STATUS_HOME = 1
GSM_REG_STATUS_SEARCHING = 2
GSM_REG_STATUS_DENIED = 3
GSM_REG_STATUS_UNKNOWN = 4
GSM_REG_STATUS_ROAMING = 5

def TimestampLog(message):
    print (unicode(datetime.datetime.now()) + message)

class FakeError(RuntimeError):
    pass


class OperationInitiated(DBusException):
    _dbus_error_name = 'org.chromium.ModemManager.Error.OperationInitiated'
    include_traceback = False


class ConnectError(DBusException):
    _dbus_error_name = 'org.chromium.ModemManager.Error.ConnectError'
    include_traceback = False


class SIM(object):
    """ SIM Object

        Mock SIM Card and the typical information it might contain.
        SIM cards of different carriers can be created by providing
        the MCC, MNC, operator name, and msin.  SIM objects are passed
        to the Modem during Modem initialization.
    """
    DEFAULT_MCC = '310'
    DEFAULT_MNC = '090'
    DEFAULT_OPERATOR = 'AT&T'
    DEFAULT_MSIN = '1234567890'

    MCC_LIST = {
        'us' : '310',
        'de' : '262',
        'es' : '214',
        'fr' : '208',
        'gb' : '234',
        'it' : '222',
        'nl' : '204',
    }

    def __init__(self, mcc_country='us',
                 mnc=DEFAULT_MNC,
                 operator_name=DEFAULT_OPERATOR,
                 msin=DEFAULT_MSIN,
                 mcc=None):
        if mcc:
            self.mcc = mcc
        else:
            self.mcc = SIM.MCC_LIST.get(mcc_country, '000')
        self.mnc = mnc
        self.operator_name = operator_name
        self.msin = msin


class Modem(dbus.service.Object):
    NOT_ACTIVATED = UInt32(0)
    ACTIVATING = UInt32(1)
    PARTIALLY_ACTIVATED = UInt32(2)
    ACTIVATED = UInt32(3)

    MM_TYPE_GSM = UInt32(1)
    MM_TYPE_CDMA = UInt32(2)

    MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY = UInt32(4)
    MM_MODEM_GSM_ACCESS_TECH_HSPA = UInt32(8)

    # Define the default sim to have AT&T carrier information

    def __init__(self, manager, name,
                 activation_state=NOT_ACTIVATED,
                 mdn='0000001234',
                 meid='A100000DCE2CA0',
                 carrier='CrCarrier',
                 esn='EDD1EDD1',
                 payment_url=('http://localhost:8080/'
                              'mobile_activation.html'),
                 usage_url='http://localhost:8080/usage.html',
                 technology=UInt32(1),
                 modem_type=MM_TYPE_CDMA,
                 device='pseudomodem0'):
        self.state = STATE_UNKNOWN
        self.name = name
        self.path = manager.path + name
        self.manager = manager
        self.activation_state = activation_state
        self.mdn = mdn
        self.meid = meid
        self.carrier = carrier
        self.esn = esn
        self.payment_url = payment_url
        self.usage_url = usage_url
        self.technology = technology
        self.modem_type = modem_type
        self.device = device
        dbus.service.Object.__init__(self, manager.bus, self.path)
        self.manager.add(self)
        self._default_signal_quality = UInt32(100)
        self._band_class = UInt32(2)
        self._band = 'F'
        self._system_id = UInt32(2004)
        self._modem_info = ('Initech', 'Modemtron 1.00', '0.42')

    def DiscardModem(self):
        """
        Discard this DBUS Object

        Send a message that a modem has disappeared
        Deregister from DBUS
        """
        TimestampLog('DiscardModem')
        self.remove_from_connection()
        self.manager.remove(self)

    @dbus.service.method(OFDP, in_signature='s', out_signature='a{sv}')
    def GetAll(self, iface, *_args, **_kwargs):
        TimestampLog('Modem: GetAll %s' % iface)
        properties = {}
        if (iface == MM + '.Modem' or
            iface == MM + '.Modem.Cdma'):
            properties = {'Device': self.device,
                      'MasterDevice': '/fake',
                      'Driver': 'fake',
                      'Type': self.modem_type,
                      'Enabled': False,
                      'Meid': self.meid,
                      'IpMethod': UInt32(2)
                      }

        return properties

    @dbus.service.method(OFDP, in_signature='ss', out_signature='v')
    def Get(self, iface, propname):
        TimestampLog('Modem: Get %s - %s' % (iface, propname))
        return self.GetAll(iface)[propname]

    @dbus.service.method(MM + '.Modem', in_signature='b', out_signature='')
    def Enable(self, on, *_args, **_kwargs):
        TimestampLog('Modem: Enable %s' % str(on))
        if on:
            self.state = STATE_ENABLED
            glib.timeout_add(50, self.OnRegistered)
        else:
            self.state = STATE_DISABLED
        return None

    def OnRegistered(self):
        TimestampLog('Modem: OnRegistered')
        if self.state == STATE_ENABLED:
            self.state = STATE_REGISTERED
            self.RegistrationStateChanged(UInt32(2), UInt32(2))

    @dbus.service.signal(MM + '.Modem.Cdma', signature='uu')
    def RegistrationStateChanged(self, cdma_1x_state, evdo_state):
        pass

    @dbus.service.method(MM + '.Modem.Simple', in_signature='',
                         out_signature='a{sv}')
    def GetStatus(self, *_args, **_kwargs):
        TimestampLog('Modem: GetStatus')
        a = { 'state': UInt32(self.state),
              'activation_state': self.activation_state,
              'carrier': self.carrier,
              'esn': self.esn,
              'mdn': self.mdn,
              'min': self.mdn,
              'payment_url': self.payment_url,
              'usage_url': self.usage_url,
              'technology': self.technology,
              }
        if self.state >= STATE_ENABLED:
            a['carrier'] = 'Test Network'
        return a

    @dbus.service.signal(MM + '.Modem', signature='uuu')
    def StateChanged(self, old_state, new_state, why):
        pass

    def ConnectDone(self, old, new, why):
        TimestampLog('Modem: ConnectDone %s -> %s because %s' % (
            str(old), str(new), str(why)))
        self.state = new
        self.StateChanged(UInt32(old), UInt32(new), UInt32(why))

    @dbus.service.method(MM + '.Modem.Simple', in_signature='a{sv}',
                         out_signature='')
    def Connect(self, _props, *_args, **kwargs):
        TimestampLog('Modem: Connect')
        if self.state != STATE_REGISTERED:
            raise ConnectError()
        delay_ms = kwargs.get('connect_delay_ms', DEFAULT_CONNECT_DELAY_MS)
        time.sleep(delay_ms / 1000.0)
        self.state = STATE_CONNECTING
        glib.timeout_add(50,
                         lambda: self.ConnectDone(self.state,
                                 STATE_CONNECTED,
                                 REASON_USER_REQUESTED))

    @dbus.service.method(MM + '.Modem')
    def Disconnect(self, *_args, **_kwargs):
        TimestampLog('Modem: Disconnect')
        self.state = STATE_DISCONNECTING
        glib.timeout_add(500,
                         lambda: self.ConnectDone(self.state,
                                 STATE_REGISTERED,
                                 REASON_USER_REQUESTED))
        raise OperationInitiated()

    @dbus.service.signal(MM + '.Modem.Cdma', signature='uua{sv}')
    def ActivationStateChanged(self, state, error, status_changes):
        pass

    def ActivateDone(self, new_activation_state, new_modem_factory, _new_mdn,
                     discard_modem_delay_ms=DISCARD_MODEM_DELAY_MS,
                     reboot_delay_ms=REBOOT_DELAY_MS):
        TimestampLog('Modem: ActivateDone: _state = %d' % new_activation_state)
        self.ActivationStateChanged(new_activation_state, 0,
                        dict(mdn=str(self.mdn),
                         min=str(self.mdn)))
        self.state = STATE_UNKNOWN
        glib.timeout_add(discard_modem_delay_ms, self.DiscardModem)
        glib.timeout_add(reboot_delay_ms, new_modem_factory)

    @dbus.service.method(MM + '.Modem.Cdma', in_signature='s',
                         out_signature='')
    def Activate(self, s, *args, **kwargs):
        # It isn't entirely clear to me how inheritence works in combination
        # with dbus method decorator magic, so lets just delegate to an impl
        # stub that subclasses can override if they like.
        self.ActivateImpl(s, args, kwargs)

    def ActivateImpl(self, _s, _args, _kwargs):
        raise NotImplementedError('Unimplemented.  Must implement in subclass.')

    def StartActivation(self, new_activation_state, factory, new_mdn):
        self.activation_state = self.ACTIVATING
        glib.timeout_add(ACTIVATION_DELAY_MS,
                         lambda: self.ActivateDone(new_activation_state,
                         factory,
                         new_mdn))
        raise OperationInitiated()

    def FailedActivateDone(self, old_activation_state, carrier_error):
        TimestampLog('Modem: FailedActivateDone')
        self.activation_state = old_activation_state
        self.ActivationStateChanged(old_activation_state,
                        carrier_error,
                        dict(mdn=str(self.mdn),
                        min=str(self.mdn)))

    def StartFailedActivation(self, carrier_error):
        old_activation_state = self.activation_state
        self.activation_state = self.ACTIVATING
        glib.timeout_add(ACTIVATION_DELAY_MS,
                         lambda: self.FailedActivateDone(old_activation_state,
                         carrier_error))
        raise OperationInitiated()

    @dbus.service.method(MM + '.Modem.Cdma', in_signature='',
                         out_signature='uu')
    def GetRegistrationState(self, *_args, **_kwargs):
        TimestampLog('Modem: GetRegistrationState')
        if self.state >= STATE_REGISTERED:
            cdma_1x_state = UInt32(2)
            evdo_state = UInt32(2)
        else:
            cdma_1x_state = UInt32(0)
            evdo_state = UInt32(0)
        return (cdma_1x_state, evdo_state)

    @dbus.service.method(MM + '.Modem.Cdma', in_signature='',
                         out_signature='u')
    def GetSignalQuality(self, *_args, **_kwargs):
        TimestampLog('Modem: GetSignalQuality')
        return self._default_signal_quality

    @dbus.service.method(MM + '.Modem.Cdma', in_signature='',
                         out_signature='(usu)')
    def GetServingSystem(self, *_args, **_kwargs):
        TimestampLog('Modem: GetServingSystem')
        return (self._band_class, self._band, self._system_id)

    @dbus.service.method(MM + '.Modem.Cdma', in_signature='',
                         out_signature='s')
    def GetEsn(self, *_args, **_kwargs):
        TimestampLog('Modem: GetEsn')
        return self.esn

    @dbus.service.method(MM + '.Modem', in_signature='',
                         out_signature='(sss)')
    def GetInfo(self, *_args, **_kwargs):
        TimestampLog('Modem: GetInfo')
        return self._modem_info


class GSM_Modem(Modem):
    def __init__(self, *args, **kwargs):
        gsm_sim = kwargs.get('gsm_sim', SIM())
        kwargs.pop('gsm_sim')
        smses = kwargs.get('smses', SIM())
        kwargs.pop('smses')
        kwargs['modem_type'] = Modem.MM_TYPE_GSM
        Modem.__init__(self, *args, **kwargs)
        self.gsm_sim = gsm_sim
        self.smses = smses
        self._imei = '490154203237518'

    @dbus.service.method(OFDP, in_signature='s', out_signature='a{sv}')
    def GetAll(self, iface, *args, **kwargs):
        TimestampLog('GSM_Modem: GetAll %s' % iface)
        properties = {}
        if (iface == MM + '.Modem.Gsm.Network'):
            properties = {
                'AllowedMode' : Modem.MM_MODEM_GSM_ALLOWED_MODE_3G_ONLY,
                'AccessTechnology' : Modem.MM_MODEM_GSM_ACCESS_TECH_HSPA
            }
        elif (iface == MM + '.Modem.Gsm.Card'):
            properties = {
                'EnabledFacilityLocks' : UInt32(0)
            }
        else:
            properties = Modem.GetAll(self, iface, args, kwargs)

        return properties


    @dbus.service.method(MM + '.Modem.Gsm.Card', in_signature='',
                         out_signature='s')
    def GetImei(self, *_args, **_kwargs):
        TimestampLog('Gsm: GetImei')
        return self._imei

    @dbus.service.method(MM + '.Modem.Gsm.Card', in_signature='',
                         out_signature='s')
    def GetImsi(self, *_args, **_kwargs):
        TimestampLog('Gsm: GetImsi')
        return self.gsm_sim.mcc + self.gsm_sim.mnc + self.gsm_sim.msin

    @dbus.service.method(MM + '.Modem.Gsm.Network', in_signature='s',
                         out_signature='')
    def Register(self, s, *args, **kwargs):
        pass

    @dbus.service.signal(MM + '.Modem.Gsm.Network', signature='(uss)')
    def RegistrationInfo(self, arr, *args, **kwargs):
        pass

    @dbus.service.signal(MM + '.Modem.Gsm.Network', signature='u')
    def NetworkMode(self, u, *args, **kwargs):
        pass

    @dbus.service.method(MM + '.Modem.Gsm.Network', in_signature='',
                         out_signature='s')
    def SetApn(self, *args, **kwargs):
        pass

    @dbus.service.method(MM + '.Modem.Gsm.Network', in_signature='',
                         out_signature='u')
    def GetSignalQuality(self, *_args, **_kwargs):
        TimestampLog('Network: GetSignalQuality')
        return 100.0

    @dbus.service.method(MM + '.Modem.Gsm.Network', in_signature='',
                         out_signature='(uss)')
    def GetRegistrationInfo(self, *_args, **_kwargs):
        TimestampLog('GSM: GetRegistrationInfo')
        registration_status = GSM_REG_STATUS_HOME

        return (registration_status, self.gsm_sim.mcc + self.gsm_sim.mnc,
                self.gsm_sim.operator_name)

    def AddSMS(self, sms):
        TimestampLog('Adding SMS to list')
        self.smses[sms['index']] = sms
        self.SmsReceived(sms['index'], True)

    @dbus.service.method(MM + '.Modem.Gsm.SMS', in_signature='',
                         out_signature='aa{sv}')
    def List(self, *_args, **_kwargs):
        TimestampLog('Gsm.SMS: List')
        return self.smses.values()

    @dbus.service.method(MM + '.Modem.Gsm.SMS', in_signature='u',
                         out_signature='')
    def Delete(self, index, *_args, **_kwargs):
        TimestampLog('Gsm.SMS: Delete %s' % index)
        del self.smses[index]

    @dbus.service.method(MM + '.Modem.Gsm.SMS', in_signature='u',
                         out_signature='a{sv}')
    def Get(self, index, *_args, **_kwargs):
        TimestampLog('Gsm.SMS: Get %s' % index)
        return self.smses[index]

    @dbus.service.signal(MM + '.Modem.Gsm.SMS', signature='ub')
    def SmsReceived(self, index, complete):
        pass

    def ActivateImpl(self, _s, _args, _kwargs):
        raise NotImplementedError('Unimplemented.  Must implement in subclass.')

class ModemManager(dbus.service.Object):
    def __init__(self, bus, path):
        self.devices = []
        self.bus = bus
        self.path = path
        dbus.service.Object.__init__(self, bus, path)

    def add(self, dev):
        TimestampLog('ModemManager: add %s' % dev.name)
        self.devices.append(dev)
        self.DeviceAdded(dev.path)

    def remove(self, dev):
        TimestampLog('ModemManager: remove %s' % dev.name)
        self.devices.remove(dev)
        self.DeviceRemoved(dev.path)

    @dbus.service.method('org.freedesktop.ModemManager',
                         in_signature='', out_signature='ao')
    def EnumerateDevices(self, *_args, **_kwargs):
        devices = map(lambda d: d.path, self.devices)
        TimestampLog('EnumerateDevices: %s' % ', '.join(devices))
        return devices

    @dbus.service.signal(MM, signature='o')
    def DeviceAdded(self, path):
        pass

    @dbus.service.signal(MM, signature='o')
    def DeviceRemoved(self, path):
        pass


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    _ = dbus.service.BusName(CMM, bus)
    manager = ModemManager(bus, OCMM)
    # The Modem constructor registers itself with the manager.
    Modem(manager, '/TestModem/0')

    mainloop = gobject.MainLoop()
    TimestampLog("Running test modemmanager.")
    mainloop.run()

if __name__ == '__main__':
    main()
