
About
-----

A simple port scanner written in C++ by Keith Craigie (https://kcraigie.com).  Uses poll() for single-threaded concurrency.

Usage
-----

kcportscan -4 ipv4_address[/prefix] [-t timeout_ms] [-s services_file]

Parameters
----------

 * *ipv4_address* The IPv4 address you want to portscan.  Use */prefix* to specify an address block.
 * *timeout_ms* Timeout for connections in milliseconds.  Defaults to 3000ms.
 * *services_file* Path to a file that maps service names to ports.  Defaults to /etc/services.

Notes
-----

To get more concurrent connection attempts, use "ulimit -n *bigger_number*" before running kcportscan.

Testing
-------

Run ./test_kcportscan.sh to perform an automated test of kcportscan using listeners on random ports.  The test returns 0 if all random ports were discovered by kcportscan and 1 if any were missed.
