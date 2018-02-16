### Tests ###

The driver was tested using the tool serperf, with messages of different sizes
under every possible option (using read/write or ioctl with and without the
waiting flag). For that reason, there are availble 6 scripts:

* testcpu.sh
* testcpuiow.sh
* testcpuio.sh
* testcpuloop.sh
* testcpuloopiow.sh
* testcpuloopio.sh

The tests use /dev/serial0 and /dev/serial1, be sure to connect to those
devices. The *loop* tests only use /dev/serial0, be sure to connect the Rx line
to the Tx one in this device, or enable the internal loopback by writing in the
corresponding file -> `loopback_toggle`.

The *loop* tests are equivalent to the others except for that they use threads
to test the driver in a single uart, they don't have the client/server dynamic,
it is just one uart sending, receiving and checking CRC.

In any case, the test will try to send and receive msgs of different lengths for
30 seconds each. The lengths to be tested are:

* 20 bytes
* 100 bytes
* 512 bytes
* 1024 bytes
* 4096 bytes
* 16384 bytes
* 32768 bytes (not on every script)
* 65536 bytes (not on every script)
* 131072 bytes

It takes ~22 minutes to finish running all the scripts.

The testing has shown to be consistent, only failing for msgs bigger than ~50k
where an overflow in the reception software fifo may occur due to the
inability of the kernel of emptying it on time.

Previously we had experienced an overrun error in prolonged transmissions but
we couldn't replicate that in the last update of the driver.
