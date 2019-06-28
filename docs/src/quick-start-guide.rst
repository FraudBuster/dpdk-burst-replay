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

For Debian::

   sudo apt-get install dpdk-dev libnuma-dev

NB: libpcap is not required, as dpdk-replay process pcap files manually.

To manually compile with a custom DPDK version, see :ref:`installing_custom_dpdk_version`.

Compiling and installing it
---------------------------

Standard way, using autotools::

  autoreconf -i && ./configure && make && sudo make install

To use the static Makefile with your custom DPDK version, see :ref:`linking_custom_dpdk_version`.

Binding wanted DPDK ports
-------------------------

First, you need to unbind the ports you want to use from the Kernel to bind them to DPDK.

To list the available ports::

  $> dpdk-devbind.py --status

  [...]
  Network devices using DPDK-compatible driver
  ============================================
  <none>

  Network devices using kernel driver
  ===================================
  0000:01:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth0 drv=tg3 unused=igb_uio *Active*
  0000:01:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth1 drv=tg3 unused=igb_uio
  0000:02:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth2 drv=tg3 unused=igb_uio
  0000:02:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth3 drv=tg3 unused=igb_uio
  0000:04:00.0 'Ethernet Controller 10-Gigabit X540-AT2 1528' if=dpdk0 drv=ixgbe unused=igb_uio *Active*
  0000:04:00.1 'Ethernet Controller 10-Gigabit X540-AT2 1528' if=dpdk0 drv=ixgbe unused=igb_uio *Active*
  0000:05:00.0 '82599ES 10-Gigabit SFI/SFP+ Network Connection 10fb' if=jenkins1 drv=ixgbe unused=igb_uio *Active*
  0000:05:00.1 '82599ES 10-Gigabit SFI/SFP+ Network Connection 10fb' if=jenkins1 drv=ixgbe unused=igb_uio *Active*
  [...]

Here, we have two NICs of two ports each, the **X540** and the **82599ES**. Their identification are their
PCI bus addresses. To bind the four ports to DPDK::

  $> dpdk-devbind -b igb_uio 04:00.0 04:00.1 05:00.0 05:00.1

Check that the ports have been well binded::

  $> dpdk-devbind.py --status

  [...]
  Network devices using DPDK-compatible driver
  ============================================
  0000:04:00.0 'Ethernet Controller 10-Gigabit X540-AT2 1528' drv=igb_uio unused=ixgbe
  0000:04:00.1 'Ethernet Controller 10-Gigabit X540-AT2 1528' drv=igb_uio unused=ixgbe
  0000:05:00.0 '82599ES 10-Gigabit SFI/SFP+ Network Connection 10fb' drv=igb_uio unused=ixgbe
  0000:05:00.1 '82599ES 10-Gigabit SFI/SFP+ Network Connection 10fb' drv=igb_uio unused=ixgbe

  Network devices using kernel driver
  ===================================
  0000:01:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth0 drv=tg3 unused=igb_uio *Active*
  0000:01:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth1 drv=tg3 unused=igb_uio
  0000:02:00.0 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth2 drv=tg3 unused=igb_uio
  0000:02:00.1 'NetXtreme BCM5720 Gigabit Ethernet PCIe 165f' if=eth3 drv=tg3 unused=igb_uio
  [...]


For more detailed explainations on this point, please refer to the related
`DPDK documentation <https://doc.dpdk.org/guides-18.11/linux_gsg/linux_drivers.html#binding-and-unbinding-network-ports-to-from-the-kernel-modules>`_.

Launching it
------------

Tool usage::

  dpdk-replay [OPTIONS] PCAP_FILE PORT1[,PORTX...]
     PCAP_FILE: the file to send through the DPDK ports.
     PORT1[,PORTX...] : specify the list of ports to be used (pci addresses).
     Options:
     --numacore <NUMA-CORE> : use cores from the desired NUMA. Only
       NICs on the selected numa core will be available (default is 0).
     --nbruns <1-N> : set the wanted number of replay (1 by default).
     --wait-enter: will wait until you press ENTER to start the replay (asked
       once all the initialization are done).

Basic example to replay **foobar.pcap** on the **04:00.0** DPDK port::

  $> dpdk-replay foobar.pcap 04:00.0


Replay a **thousand** times the **foobar.pcap** file on **four** DPDK ports.::

  $> dpdk-replay --nbruns 1000 foobar.pcap 04:00.0,04:00.1,04:00.2,04:00.3


Use NIC ports from **NUMA 1**::

  $> dpdk-replay --numacore 1 foobar.pcap 81:00.0

Let dpdk-replay prepare the replay and decide when to make it starts by **pressing ENTER**::

  $> dpdk-replay --wait-enter foobar.pcap 04:00.0
