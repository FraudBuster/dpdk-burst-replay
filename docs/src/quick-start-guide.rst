..  dpdk-burst-replay: BSD-3-Clause
    Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

.. _quick-start-guide:

Quick start guide
=================

Install dependencies
--------------------

With your distro package manager, install these packages (names can change a bit):

* dpdk-dev (obsly)
* libnuma-dev

::
   
   sudo apt-get install dpdk-dev libnuma-dev

NB: libpcap is not required, as dpdk-replay process pcap files manually.

To manually compile with a custom DPDK version, see :ref:`installing_custom_dpdk_version`.

Compiling and installing it
---------------------------

Standard way, using autotools::

  autoreconf -i && ./configure && make && sudo make install

OR, using a static makefile with DPDK's env and conf::

  RTE_SDK=<RTE_SDK_PATH> make -f DPDK_Makefile && sudo cp build/dpdk-replay /usr/bin

Launching it
------------

::

  dpdk-replay [--nbruns NB] [--numacore 0|1] FILE NIC_ADDR[,NIC_ADDR...]

Example::

  dpdk-replay --nbruns 1000 --numacore 0 foobar.pcap 04:00.0,04:00.1,04:00.2,04:00.3
