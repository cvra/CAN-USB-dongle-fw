# CAN-dongle protocol

## Overview

The protocol between the dongle and the PC client consists of unidirectional messages and bidirectional
service calls with a request and a response message. Messages/service calls are encoded in the
[datagram-messages](https://github.com/Stapelzeiger/datagram-messages) format, as described
[here](https://github.com/Stapelzeiger/datagram-messages/blob/master/spec/message.md).

All messages are packed into a [serial-datagram](https://github.com/Stapelzeiger/serial-datagram).

### Messages:

Name | Description
-----|------------
'rx' | Receive a CAN frame with a timestamp of reception.
'drop' | Notify client of dropped CAN frames due to full buffers.
'err' | Notify client of detected error frame on the bus.

### Servicd Calls:

Name | Description
-----|------------
'tx' | Send CAN frames.
'bit rate' | Configure the CAN bitrate
'filter' | Configure an acceptance filter
'bus voltage' | Measure the 5V CAN bus voltage.
'bus power' | Enable/disable 5V bus power supply from USB 5V.
'silent' | Enter silent mode (reception only, no transmissions, no acknowledgement).
'loop back' | Enter loop back mode (all sent frames are also received).
'name' | Device name
'hw version' | Hardware version
'sw version' | Software version


## Message encoding

Notation:

* `[]` MessagePack array
* `{KEY: VALUE}` MessagePack dict
* `''` MessagePack string
* `uint` MessagePack unsigned integer
* `bin` MessagePack binary bin type
* `bool` MessagePack boolean

## Messages CAN-dongle > PC

### Recive CAN frame

The timestamp is in microsecond of the time of reception
(CAN_FRAME encoding is described below.)

```
name: 'rx'
argument: [CAN_FRAME, uint timestamp]
```

### Dropped CAN frames
Indicate lost received CAN frames due to full buffers.

```
name: 'drop'
argument: nil
```

### Error frame
Indicate the reception of an error frame.

```
name: 'err'
argument: uint timestamp
```

## Service calls PC > CAN-dongle > PC

### Send CAN frames
Send an array of N CAN frames.
Receive true if the CAN frames are successfully placed in the output buffer.
(CAN_FRAME encoding is described below.)

```
name: 'tx'
request: [CAN_FRAME_1, ... CAN_FRAME_N]
response: bool ok
```
### Bit rate
```
name: 'bit rate'
request: uint bit rate (default: 1Mbit)
response: bool ok
```

### Acceptance filter

The acceptance filter is used to filter out received CAN frames to simplify the client
application and to reduce the load on the dongle.

Retruns a boolean and a timestamp.
The boolean indicates a successful service call.
All received CAN frames with a timestamp greater than the retruned one are guaranteed to have passed the filter.
(This can occur if already received CAN frames were being forwarded while the filter was being configured.)

The encoding for ACCEPTANCE_FILTER is described below.

```
name: 'filter'
request: ACCEPTANCE_FILTER
     (default: accept all frames)
response: [bool ok, uint timestamp]
```

### Bus voltage
Measure the 5V CAN bus voltage.

```
name: 'bus voltage'
request: nil
response: float voltage
```

### Bus power
Enable/disable 5V bus power from USB.

Set the argument to `true` to enable.
Note: If bus voltage is not 0, bus power cannot be enabled.

```
name: 'bus power'
request: bool enable (default: disabled)
response: nil
```

### Silent mode
In silent mode, no CAN frames can be sent and the dongle does not acknowledge received frames.
This mode is useful for silent monitoring an active CAN bus.

```
name: 'silent'
request: bool enable (default: enabled)
response: nil
```

### Loop back mode
```
name: 'loop back'
request: bool enable (default: enabled)
response: nil
```

### Hardware version
```
name: 'hw version'
request: nil
response: string version
```

### Software version
```
name: 'sw version'
request: nil
response: string version
```

### Device name
This helps to distinguish several attached devices.

```
name: 'name'
request: nil
response: string name
```

## CAN_FRAME encoding

MessagePack bin type format, LSByte first:

* 32 bit: ID (see ID encoding)
* 0-8 bytes: data

Note: if the CAN frame is a remote transmission request, then the data length
corresponds to the requested data length code (DLC). The data content is not
defined and should be set to 0.

## ID encoding

A CAN ID is represented by a 32bit unigned integer, where it's fields have following bitwise encoding:

bits   | meaning
-------|---------------
[0-10] | standard CAN ID (if extended ID flag cleared, bit 11-28 set to 0)
[0-28] | extended CAN ID (if extended ID flag set)
[29]   | extended ID flag
[30]   | remote transmission request flag (RTR)
[31]   | unused, set to 0

## ACCEPTANCE_FILTER encoding

For a CAN frame to be passed on to the client it has to pass an acceptance filter.
The filter consists of one or more entries, to which a received CAN frame is compared.
The frame passes the filter if it matchs with at least one filter entry.

A filter entry is composed of a pair of two CAN IDs, a filter and a mask (as described above).
On every bit where the mask is 1, the ID bits have to match the filter ID.
This is equivalent to the following bit operation and comparison:

```
((id XOR filter_id) AND filter_mask) == 0
```

The filter is encoded as follows:

```
[[uint ID_1, uint MASK_1], ... [uint ID_N, uint MASK_N]]
```

Note: a higher number of entries should not have an inpact on the performance of the dongle, since
filters should be supported by the hardware.
