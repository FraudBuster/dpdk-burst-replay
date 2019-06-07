..  dpdk-burst-replay: BSD-3-Clause
    Copyright 2019 Jonathan Ribas, FraudBuster. All rights reserved.

.. _introduction:

Introduction
============

The tool is designed to provide high DPDK performances to burst any pcap dump on
a single or multiple NIC port(s), while keeping it simple to use.

It's a Linux command line tool, wrote in C and compiling on both x86_64 and arm64.
It mainly use DPDK, but rely also on libnuma, which are the only tool dependencies.

The tool usage tend to looks like tcpreplay in term of simplicity. Only two args are required:
one pcap file and one DPDK port address to replay on:

::

   $> dpdk-replay foo.pcap 04:00.0

What can it do?
---------------

* Load a pcap file: standard input format for all common dumping/analyzing/replaying tools like
  tcpdump, tcpreplay, wireshark etc...
* Put all loaded packets in cache(s) to avoid reading the pcap file while its been replays.
* Burst packets simultaneously on multiple ports (ATM, only NICs on the same NUMA can be addressed).
* Run the same pcap several times in a row. It can be useful to extend a stress test without having
  to provide a bigger pcap file.
* At the end, some stats are displayed for each ports (time spent, pkts-per-sec, total bitrate etc...)
* Abstract DPDK stack: All EAL/mempool/ports initializations are automatically handled.

.. _dpdk_compatible_versions:

DPDK compatible versions
------------------------

dpdk-burst-replay releases are compatible with DPDK LTS versions.

+----------------------------+---------------------------+
| dpdk-burst-replay releases | Tested with DPDK versions |
+============================+===========================+
| v1.1.0                     | 16.11.9/17.11.5/18.11.1   |
+----------------------------+---------------------------+

NB: Other intermediate versions should also works. Last versions can need some adaptation.

Ok, let's start!
----------------

If you want try it, please have a look on the :ref:`quick-start-guide` and :ref:`user-guide`:

.. toctree::
   :maxdepth: 2

   quick-start-guide
   user-guide

If you're looking for the GIT, have a look `here <https://git.dpdk.org/apps/dpdk-burst-replay/>`_.

And if you find a bug, want to share a suggestion or see what are the opened issues,
please check the `Bugzilla <https://bugs.dpdk.org/describecomponents.cgi?product=dpdk-burst-replay>`_.


FOSDEM 2019 presentation
------------------------

A presentation have been made at FOSDEM'19. You can find bellow the event info, slides and video links:

* Event info: `FOSDEM site <https://fosdem.org/2019/schedule/event/dpdk_burst_replay/>`_.
* Video: `FOSDEM <https://video.fosdem.org/2019/H.2214/dpdk_burst_replay.mp4>`_ or `Youtube <https://www.youtube.com/watch?v=j56Y-SPRDE0>`_.
* Slides: `PDF file <https://fosdem.org/2019/schedule/event/dpdk_burst_replay/attachments/slides/2874/export/events/attachments/dpdk_burst_replay/slides/2874/presentation.pdf>`_.


BSD Licence
-----------

BSD 3-Clause License

Copyright (c) 2018, Jonathan Ribas, FraudBuster. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
