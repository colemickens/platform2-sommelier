# Midis: MIDI Service

## ABOUT

Midis (MIDI service) is a system service for processing MIDI(https://en.wikipedia.org/wiki/MIDI)
events. It can communicate information about device connection/disconnection to several client
applications, and pass file descriptors to these clients to send and receive MIDI messages to
and from hardware, respectively.

## KEYWORDS

Client - A userspace process that establishes an IPC connection with midis. Once the connection is
established, it can listen for device connection / disconnection messages sent from midis, and can
also request for file descriptors to listen to different MIDI H/W devices and write to them.

Device - Representation of a MIDI h/w device in midis. It is considered analogous to the ALSA
sequencer concept of a "client". A device consists of multiple subdevices (referred to in ALSA
sequencer parlance as "ports").

## IMPORTANT MISC CLASSES

SeqHandlerBase - Interface which is used to perform all interactions with the
ALSA sequencer interface. Each object of class DeviceTracker requires *one*
implementation of SeqHandlerBase to be supplied to it.

DeviceTracker - Performs management and book-keeping of devices. Watches the
ALSA sequencer interface for notifications about device
connection / disconnection, and accordingly creates / destroys objects of class
Device. Also handles all incoming MIDI data *from* clients to H/W and
vice-versa and invokes the requisite callbacks to ensure the data is sent
correctly.

ClientTracker - Performs management and book-kepeing of clients. Watches the
IPC interface for incoming client connection requests and accordingly creates
objects of class Client when required. Also provides a means to remove clients
either due to untimely disconnection or during shutdown.
