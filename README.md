## Serperf serial device test tool

[![Build Status](https://travis-ci.org/HernanG234/serperf.svg?branch=hernan/serperf-crc)](https://travis-ci.org/HernanG234/serperf#)

### Intro

The tool consists on a program that manages the data in messages. This messages
are formed by a header (with type, length and crc) and the payload. The
user can choose between launching the server or the client.

The server can wait until there's a message to receive and retransmit that
(ping-pong mode) or it can send a specific number of bytes requested by the
client (req-bytes mode). The server answer's type depends on what the client
sends on its message's header.

The client sends a message (in this case 0x55) and receives a reply with 3
modes:

1. With a defined number of messages
2. Over a finite amount of time
3. Ad infinitum (Default)

```
   --------------                                           ---------------
  |    Server    |                                         |    Client     |
   --------------        Pins                  Pins         ---------------
    |        ▴   Rx:11 Tx:13 GPIO:      Rx:22 Tx:21 GPIO:     ▴        |
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

Clone from [here](https://github.com/HernanG234/serperf). Run make (Make it portable).

### Usage

Arguments:

```
-s -> run as server
-c -> run as client
-l -> (optional - default = 1024) length of message to send in bytes
-x -> (optional) type of message: PING_PONG = 0 (default) | REQ_BYTES = 1
-r -> (optional - default = 0) Number of bytes requested to server
-m -> (optional) (client mode 1) number of messages to send as client
-t -> (optional) (client mode 2) seconds that the client will work
```

Message length (-l) only accepts values between 0 and 1024. If no -m or -t is
passed as argument the client will work infinitely. If -x is passed as an
argument, the message length is 4 (-l has no effect). -r only works if the
value sent with -x equals 1. 

Server doesn't need extra arguments.

### Examples

Run server:

	> #./serperf -s /dev/serial0

Run client (mode 1): Client works in req-bytes mode asking for 1000 messages of
4kB to the server.

	> #./serperf -c -x 1 -r 4096 -m 1000 /dev/serial1

Run client (mode 2): Client works in ping-pong mode for 300 seconds sending and
receiving messages of 1024 bytes.

	> #./serperf -c -x 0 -t 300 /dev/serial1

Run client (mode 3): Client works infinitely in ping-pong mode sending and
receiveing messages of 64 bytes.

	> #./serperf -c -l 64 /dev/serial1

### TO DO

- [x] Readme.md
- [x] Add crc
- [ ] Makefile portable (should work native and cross build)
- [ ] travis.yml
