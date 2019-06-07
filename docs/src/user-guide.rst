..  dpdk-burst-replay: BSD-3-Clause
    Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

.. _user-guide:

User guide
==========

.. _installing_custom_dpdk_version:

Installing custom DPDK version
------------------------------

You should install a custom DPDK version if:

* Your distribution does not propose any DPDK package (obsly).
* You want to use a different version of what is proposed in your distro.
* Your NICs are not supported by your distro package (which can be the case for vendors
  like Mellanox/Broadcom etc).
* Or, you want to tune :ref:DPDK options.

First, note that dpdk-burst-replay is compatible (ie, checked) with DPDK LTS versions.
Please refer to :ref:`dpdk_compatible_versions` section.

Please also note that this doc section is not intended to replace the
`official DPDK compiling guide <https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html>`_,
but is more focused on giving additional tips related to dpdk-burst-replay purposes.

Get DPDK
^^^^^^^^

Once you have choose the version you want to use, get its tarball from `download section <https://core.dpdk.org/download/>`_ or directly from `GIT repository <https://git.dpdk.org/dpdk-stable/>`_.

For example::

  $> wget https://fast.dpdk.org/rel/dpdk-18.11.1.tar.xz && tar xvf dpdk-18.11.1.tar.xz


.. _customize_dpdk:

Customize DPDK
^^^^^^^^^^^^^^

DPDK build options are mainly located on **config/common_base** file. Here are some example that could be helpful:

* In lasts versions of DPDK the memory management have changed and there is an hardcoded limit of memory
  size that can be allocated. And, as dpdk-burst-replay put in cache all pcap packets before sending them
  it could be an issue if you want to replay big pcap dumps. To be able to use up to 128Go (instead of 32Go
  by default), set **131272** here::

    CONFIG_RTE_MAX_MEM_MB_PER_LIST=32768

* By default DPDK libs are compiled in static, if you want them to be shared ones, set to 'y' this line::

    CONFIG_RTE_BUILD_SHARED_LIB=n

Build DPDK
^^^^^^^^^^

Once your configurations have been made, you can follow the
`official DPDK compiling guide <https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html>`_. For example::

  $> make config T=x86_64-native-linux-gcc
  $> make -j99 T=x86_64-native-linux-gcc

Install DPDK
^^^^^^^^^^^^

To install compiled DPDK version into /usr/local, just type::

  $> sudo make install

NOTE: You can change the destination folder by specifying a different **DESTDIR**::

  $> make DESTDIR=~/dpdk-18.11.1 install


.. _linking_custom_dpdk_version:

Linking custom DPDK version
---------------------------

Autotools way
^^^^^^^^^^^^^

You can specify your custom DPDK version location on **configure** step, for example::

  $> ./configure CFLAGS=-I/home/user/dpdk-18.11.1/usr/local/include/dpdk LDFLAGS=-L/home/user/dpdk-18.11.1/usr/local/lib

Then, compile and install as usual::

  $> make && sudo make install

Static way
^^^^^^^^^^

Using a static makefile with DPDK's env and conf::

  RTE_SDK=<DPDK_PATH> make -f DPDK_Makefile && sudo cp build/dpdk-replay /usr/bin

Troubleshooting
---------------

TODO :)
