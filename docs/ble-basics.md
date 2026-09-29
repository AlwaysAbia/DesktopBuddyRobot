# BLE basics for this project

Plain-language notes on the Bluetooth terms that come up when testing the robot.
The exact UUIDs and payloads are in [../interface-contract.md](../interface-contract.md), section 1.

## BLE vs "Bluetooth" in the phone settings

Bluetooth Low Energy (BLE) is a different mode from classic Bluetooth (headphones, speakers,
keyboards). The phone's Bluetooth settings screen mostly handles classic devices. The robot
shows up there because it advertises, but connecting to it needs an app that speaks BLE
(nRF Connect for testing, the companion app later).

## GATT

**GATT** (Generic Attribute Profile) is how a BLE device lays out its data so that any phone can
read and write it.

- A **service** is a group of related values. The robot has one, `9b370000-…`.
- A **characteristic** is one value inside a service that the phone can read, write, or
  subscribe to. The robot has five: WiFi Config, Eye Color, Mode Select, OTA Check Trigger, Status.
- A **UUID** is the long ID that names a service or characteristic. Custom ones like ours have no
  official name, so tools show "Unknown" unless the device also provides a description (the
  robot does, since 0.5.1).
- **Properties** say what the phone may do with a characteristic: Read, Write, Notify.
- **Notify** means the robot pushes a new value to the phone by itself, after the phone has
  subscribed (the triple-arrow button in nRF Connect). Only Status has it.

The robot is the GATT **server** (it holds the data); the phone is the **client** (it asks for
and sets values).

## MTU

**MTU** (Maximum Transmission Unit) is the largest single message a BLE connection carries. Every
connection starts at 23 bytes, which leaves 20 bytes of actual data (3 bytes are protocol
overhead). The phone and robot can agree on a bigger one, and the phone must ask for it:

- nRF Connect on Android: three-dot menu on the connected device, **Request MTU**, enter `247`.
- iOS negotiates it automatically.

Why it matters here: the Status JSON is about 100 bytes and a WiFi Config write can be about
100 bytes. At the default MTU, notifications of that size do not fit. The robot then skips the
notification instead of sending it cut off, so Status only changed when you pressed Read (a Read
is split across several messages automatically). With MTU 247 the notifications arrive.
The companion app must request an MTU of at least 128 right after connecting.

## Pairing

The robot uses "Just Works" pairing: no PIN, and nothing is remembered on the robot. Only the
WiFi Config write asks the phone for an encrypted link, which is why the pairing prompt appears
the first time you write the WiFi credentials.
