#ifndef PWOSPF_PACKET_HANDLER_H
#define PWOSPF_PACKET_HANDLER_H

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_packethandler.h"
#include "sr_pwospf.h"
#include "sr_rt.h"


struct pwospf_router
{
	uint32_t rid; // by convention, it's the IP addr of 0th interface 
	uint32_t aid;
	uint16_t lsuint; // interval in seconds between link state update broadcasts.
	uint32_t last_seq;  // the sequence number of the last recieved LSU packets. 
	struct pwospf_interface *ifs; // list of current router interfaces
	struct pwospf_router *next;
};

struct pwospf_interface
{
	char name[SR_IFACE_NAMELEN]; // used only for self router
	uint32_t ip_addr;
	uint32_t mask;
	uint16_t helloint; // interval in seconds between HELLO broadcasts
	uint32_t neighbor_rid; // ID of the neighboring router.
	uint32_t neighbor_ip_addr;
	time_t ts;  // used only for self router, compare time of hello message with this, for checking timeout
	struct pwospf_interface *next;
};

struct q_node
{
	uint32_t rid;
	uint32_t nexthop;
	char interface[SR_IFACE_NAMELEN];
	struct q_node *next;
};

void handle_pwospf_packet(	struct sr_instance* sr,
							uint8_t * packet/* lent */,
							unsigned int len,
                			char* interface);


void update_routing_table(struct sr_instance* sr);
void handle_pwospf_hello(uint8_t * packet, char* interface);
struct pwospf_router * get_router_with_rid(uint32_t rid);
void forward_lsu(struct sr_instance* sr, uint8_t * packet,unsigned int len, char* interface);
void handle_pwospf_lsu(uint8_t * packet, char* interface,struct sr_instance* sr, unsigned int len);


#endif