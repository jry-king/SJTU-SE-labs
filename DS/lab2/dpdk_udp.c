/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

struct ether_addr ethaddr;			// ethernet port MAC address

/*
 * calculate the checksum of a given series of bytes according to the Internet checksum
 * i.e. append a 0 if the number of bytes is odd, take every 2 bytes as a 16-bit integer and add them up
 * if the result exceeds 16 bits, add the first 16 bits to the last 16 bits repeatedly till it has no more than 16 bits
 * then take its complement's last 16 bit as result
 */
static uint32_t checksum(unsigned char *data, unsigned int len)
{
	unsigned int i;
	unsigned int temp = len - (len % 2);
	uint32_t cksum = 0;
	for (i = 0; i < temp; i += 2)
	{
		cksum += (uint16_t)ntohs(*(uint16_t*)(data+i));
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}
	if (i < len)
	{
		cksum += data[i] << 8;
		cksum = (cksum >> 16) + (cksum & 0xffff);
	}
	return cksum;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count())
		return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	ethaddr = addr;
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * The lcore main. This is the main function that does the work,
 * continuously constructing and sending UDP/IP packets
 */
static __attribute__((noreturn)) void
lcore_main(struct rte_mempool *mbuf_pool)
{
	const uint8_t nb_ports = rte_eth_dev_count();
	uint8_t port;

	/*
	 * Check that the port is on the same NUMA node as the polling thread
	 * for best performance.
	 */
	for (port = 0; port < nb_ports; port++)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());

	/* Run until the application is quit or killed. */
	for (;;) 
	{
		char data[9] = "testdata\0";
		struct ether_hdr *ethernet_header = NULL;
		struct ipv4_hdr *ip_header = NULL;
		struct udp_hdr *udp_header = NULL;
		char *payload = NULL;

		struct rte_mbuf *buf;
		buf = rte_pktmbuf_alloc(mbuf_pool);
		buf->next = NULL;
		buf->data_len = sizeof(*ethernet_header) + sizeof(*ip_header) + sizeof(*udp_header) + sizeof(data);
		buf->pkt_len = buf->data_len;

		ethernet_header = rte_pktmbuf_mtod_offset(buf, struct ether_hdr*, 0);
		ip_header = rte_pktmbuf_mtod_offset(buf, struct ipv4_hdr*, sizeof(struct ether_hdr));
		udp_header = rte_pktmbuf_mtod_offset(buf, struct udp_hdr*, sizeof(struct ether_hdr)+ sizeof(struct ipv4_hdr));
		payload = rte_pktmbuf_mtod_offset(buf, char*, sizeof(struct ether_hdr)+ sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));

		memcpy(payload, data, strlen(data));
		
		ethernet_header->d_addr = ethaddr;
		ethernet_header->s_addr = ethaddr;
		ethernet_header->ether_type = 0x0008;
		
		// construct IP header
		// use big endian
		ip_header->version_ihl = 0x45;						// 4-bit version (4 means ipv4) + header length (5 bytes without optional part)
		ip_header->type_of_service = 0;						// type of service, normally 0
		ip_header->total_length = 0x2500;					// total length of IP datagram, 37 bytes here (20 bytes IP header + 8 bytes UDP header + 9 bytes payload)
		ip_header->packet_id = 0;							// sequence number of packet, used in fragmentation, unnecessary here
		ip_header->fragment_offset = 0;						// packet offset in whole message, used in fragmentation, still unnecessary here
		ip_header->time_to_live = 64;						// time to live, denotes maximum number of routers a packet can pass, normally 64/128/255
		ip_header->next_proto_id = 17;						// protocol type of payload, 17 represents UDP and 6 for TCP
		ip_header->src_addr = IPv4(10, 49, 168, 192);		// source IP address (192.168.49.10 here)
		ip_header->dst_addr = IPv4(1, 49, 168, 192);		// destination IP address (192.168.49.1 here)
		ip_header->hdr_checksum = 0;
		// calculate checksum, IP checksum only checks header
		uint32_t ip_cksum = checksum((unsigned char *)ip_header, sizeof(struct ipv4_hdr));
		ip_header->hdr_checksum = htons(~ip_cksum & 0xffff);

		// construct UDP header after IP header since UDP header need source address, destination address and data
		// along with UDP header to do checksum
		// use big endian
		udp_header->src_port = 0x0a00;		// source port number, use 10 here
		udp_header->dst_port = 0x0100;		// destination port number, use 1 here
		udp_header->dgram_len = 0x1100;		// length of datagram, 17 bytes here (8 butes UDP header + 9 bytes payload)
		udp_header->dgram_cksum = 0;
		// calculate checksum, UDP checksum checks header, data and pseudoheader 
		// which consists of source address, destination address, datagram length and protocol type (17 for UDP)
		// append a 0 to the 1-byte protocol type and the pseudoheader is 12 bytes long
		uint32_t udp_cksum = 17 + (uint32_t)ntohs(udp_header->dgram_len);									// pseudoheader
		udp_cksum += checksum((unsigned char *)&ip_header->src_addr, 2 * sizeof(ip_header->src_addr));		// pseudoheader
		udp_cksum += checksum((unsigned char *)data, 9);													// data
		udp_cksum += checksum((unsigned char *)udp_header, sizeof(struct udp_hdr));							// header
		udp_cksum = (udp_cksum >> 16) + (udp_cksum & 0xffff);												// prevent overflow
		udp_header->dgram_cksum = htons(~udp_cksum & 0xffff);

		rte_eth_tx_burst(0, 0, &buf, 1);
	}
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint8_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count();
	fprintf(stderr, "%u ports detected\n", nb_ports);

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n",
					portid);

	/* Call lcore_main on the master core only. */
	lcore_main(mbuf_pool);

	return 0;
}
