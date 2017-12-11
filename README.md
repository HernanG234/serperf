## Serperf serial device test tool

[![Build Status](https://travis-ci.org/ezequielgarcia/serperf.svg?branch=master)](https://travis-ci.org/ezequielgarcia/serperf#)

### Intro

The tool consists on a program that manages the data in messages. This messages
are formed by a header (with type, length, crc /*TODO*/) and the payload. The
user can choose between launching the server or the client. The server just
waits until there's a message to receive and retransmits that (ping-pong). The
client sends a message (in this case 0x55) and receives a reply with 3 modes:

1 - With a defined number of messages
2 - Over a finite amount of time
3 - Ad infinitum (Default)


   --------------                                           ---------------
  |    Server    |                                         |    Client     |
   --------------        Pins                  Pins         ---------------
    |        ▴   Rx:11 Tx:13 GPIO:      Rx:22 Tx:21 GPIO:     ▴        |
    |        |          -------               -------         |        |
    |        |         | UART0 |             | UART1 |        |        |
    ▾        |          -------               -------         |        ▾
  ------   ------                                           ------   ------
┌-|    |   |    |<------| Rx | ⎻⎻⎻⎻⎻⎻\ /⎻⎻⎻⎻⎻⎻ | Rx |------>|    |   |    |--
| ------   ------                     X                     ------   ------  |
| |    |   |    |       | Tx | ------/ \------ | Tx |       |    |   |    |  |
| ------   ------          ▴                      ▴         ------   ------  |
| |    |   |    |          |                      |         |    |   |    |  |
| ------   ------          |                      |         ------   ------  |
|      Kfifos              |                      |              Kfifos      |
|                          |                      |                          |
 ---------------------------                       ---------------------------

###Installation

Clone from (SOMEWHERE - git@bitbucket.org:vanguardiasur/serperf.git). Run make
(Make it portable).

### Usage

Arguments:

```
-s -> run as server
-c -> run as client
-l -> (optional - default = 1024) length of message to send in bytes
-m -> (optional) (mode 1) number of messages to send as client
-t -> (optional) (mode 2) seconds that the client will work
```

If no -m or -t is passed as argument the client will work infinitely. Server
doesn't need extra arguments.

### Examples

Run server:
	>#./serperf -s /dev/serial0

Run client (mode 1):
	>#./serperf -c -l 512 -m 1000 /dev/serial1

Run client (mode 2):
	>#./serperf -c -t 300 /dev/serial1

Run client (mode 3):
	>#./serperf -c /dev/serial1

### TO DO

- [x] Readme.md
- [] Add crc
- [] Makefile portable (should work native and cross build)
- [] travis.yml
