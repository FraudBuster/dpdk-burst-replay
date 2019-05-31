..  dpdk-burst-replay: BSD-3-Clause
    Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

.. _todo-list:

TODO list
=========

* Add a configuration file or cmdline options for all code defines.
* Add an option to configure maximum bitrate.
* Add an option to send the pcap with the good pcap timers.
* Add an option to send the pcap with a multiplicative speed (like, ten times the normal speed).
* Add an option to select multiple pcap files at once.
* Be able to send dumps simultaneously on both numacores.
* Split big pkts into multiple mbufs.
* Add a Python module to facilitate scripting (something like what does scapy for tcpreplay sendpfast func).
* Manage systems with more than 2 numa cores.
* Use the maximum NICs capabilities (Tx queues/descriptors).
