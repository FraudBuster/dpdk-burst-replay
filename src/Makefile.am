# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 Jonathan Ribas, FraudBuster. All rights reserved.

bin_PROGRAMS		=	dpdk-replay
dpdk_replay_SOURCES	=	main.c \
						cpus.c \
						dpdk.c \
						pcap.c \
						utils.c

dpdk_replay_CFLAGS	=	$(CFLAGS) -I/usr/include/dpdk -march=native
dpdk_replay_LDFLAGS	=	$(LDFLAGS) -pthread -lnuma -ldpdk -lm
