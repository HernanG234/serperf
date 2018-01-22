## Serperf serial device test tool

[![Build Status](https://travis-ci.org/HernanG234/serperf.svg?branch=master)](https://travis-ci.org/HernanG234/serperf#)

### Intro

The tool consists on a program that manages the data in messages. These
messages are formed by a header (with type, length and crc) and the payload.
The user can choose between launching the server or the client.

The server loops until there's a message to receive and retransmit that
(ping-pong mode) or it can send a specific number of bytes requested by the
client (req-bytes mode). The server answer's type depends on what the client
sends on its message's header.

The client sends a message (in this case 0x55) and receives a reply, this with
can be done in 3 different modes:

1. With a defined number of messages
2. Over a finite amount of time
3. Ad infinitum (Default)

The tool also supports using the driver's ioctl interface, instead of the
standard read() and write(). This allows the user to choose whether to send
or not data with the WAIT_FOR_XMIT flags, which makes the driver wait for
transmitter shift register to be emptied (waiting is the default mode). To
avoid this extra wait just use the -i option. To use ioctl and wait use -iw. To
use standard read/write() don't add any extra option.

Verbose mode is also supported, for debugging. It consists on printing where
the tool is on every step.

```
   --------------                                           ---------------
  |    Server    |                                         |    Client     |
   --------------        Pins                  Pins         ---------------
    |        ▴   Rx:22 Tx:21 GPIO:      Rx:11 Tx:13 GPIO:     ▴        |
    |        |          -------               -------         |        |
    |        |         | UART0 |             | UART1 |        |        |
    ▾        |          -------               -------         |        ▾
  ------   ------                                           ------   ------
┌-|    |   |    |<------| Rx | ------\ /-------| Rx |------>|    |   |    |--
| ------   ------                     X                     ------   ------  |
| |    |   |    |       | Tx | ------/ \------ | Tx |       |    |   |    |  |
| ------   ------          ▴                      ▴         ------   ------  |
| |    |   |    |          |                      |         |    |   |    |  |
| ------   ------          |                      |         ------   ------  |
|      Kfifos              |                      |              Kfifos      |
|                          |                      |                          |
 ---------------------------                       ---------------------------
```

### Installation

Clone from [here](https://github.com/vanguardiasur/serperf). Run make. Remember to
export CC environment variable.

### Usage

Arguments:

```
-s | --server		-> run as server
-c | --client		-> run as client
-l | --msg-length	-> (default = 1024) length of message to send in bytes
-x | --msg-type		-> type of message: PING_PONG = 0 (default) | REQ_BYTES = 1
-r | --req-bytes	-> (default = 0) Number of bytes requested to server
-m | --messages		-> (optional) (client mode 1) number of messages to send as client
-t | --time		-> (optional) (client mode 2) seconds that the client will work
-v | --verbose		-> (optional) verbose mode
-i | --ioctl 		-> (optional) use ioctl instead of standard read/write()
-w | --wait-4-xmit 	-> (optional) set the WAIT_FOR_XMIT flag
```

Message length (-l) only accepts values between 0 and 131072. If no -m or -t is
passed as argument the client will work infinitely. If -x is passed as an
argument, the message length is 4 (-l has no effect). -r only works if the
value sent with -x equals 1, in req-bytes mode. -w only works if -i is passed
too.

Server can only take -i, -w, -v. Proc can be interrupted with Ctrl+C, shows
stats when it exits.

### Examples

Run server:

	> #./serperf -s /dev/serial0

Run server (no wait):

	> #./serperf -s -i /dev/serial0

Run client (mode 1): Client works in req-bytes mode asking for 1000 messages of
4kB to the server.

	> #./serperf -c -x 1 -r 4096 -m 1000 /dev/serial1

Run client (mode 2): Client works in ping-pong mode for 300 seconds sending and
receiving messages of 1024 bytes, through ioctl but w/waiting flag.

	> #./serperf -c -x 0 -t 300 -i -w /dev/serial1

Run client (mode 3): Client works infinitely in ping-pong mode sending and
receiveing messages of 64 bytes. Verbose mode.

	> #./serperf -c -l 64 -v /dev/serial1

