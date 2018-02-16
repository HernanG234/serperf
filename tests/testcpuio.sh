echo "starting server"
( serperf -s /dev/serial0 & )

date
echo "ping-pong 20-byte message"
serperf -c -x 0 -l 20 -t 30 -i /dev/serial1

date
echo "ping-pong 100-byte message"
serperf -c -x 0 -l 100 -t 30 -i /dev/serial1

date
echo "ping-pong 512-byte message"
serperf -c -x 0 -l 512 -t 30 -i /dev/serial1

date
echo "ping-pong 1024-byte message"
serperf -c -x 0 -l 1024 -t 30 -i /dev/serial1

date
echo "ping-pong 4096-byte message"
serperf -c -x 0 -l 4096 -t 30 -i /dev/serial1

date
echo "ping-pong 16384-byte message"
serperf -c -x 0 -l 16384 -t 30 -i /dev/serial1

date
echo "ping-pong 131072-byte message"
serperf -c -x 0 -l 131072 -t 30 /dev/serial1

killall serperf
