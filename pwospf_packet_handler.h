#ifndef PWOSPF_PACKET_HANDLER_H
#define PWOSPF_PACKET_HANDLER_H

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_packethandler.h"
#include "sr_pwospf.h"


struct pwospf_router
{
	uint32_t rid; // by convention, it's the IP addr of 0th interface 
	uint32_t aid;
	uint16_t lsuint; // interval in seconds between link state update broadcasts.
	struct pwospf_interface *ifs; // list of current router interfaces
	struct pwospf_router *next;
};

struct pwospf_interface
{
	uint32_t ip_addr;
	uint32_t mask;
	uint16_t helloint; // interval in seconds between HELLO broadcasts
	uint32_t neightbor_rid; // ID of the neighboring router.
	uint32_t neightbor_ip_addr;
	struct pwospf_interface *next;
};

void handle_pwospf_packet(	struct sr_instance* sr,
							uint8_t * packet/* lent */,
							unsigned int len,
                			char* interface);

#endif