/*-----------------------------------------------------------------------------
 * file:  sr_pwospf.h
 * date:  Tue Nov 23 23:21:22 PST 2004 
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_PWOSPF_H
#define SR_PWOSPF_H

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sr_router.h"
#include "sr_if.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_packethandler.h"


/* forward declare */
struct sr_instance;

struct pwospf_subsys
{
    /* -- pwospf subsystem state variables here -- */


    /* -- thread and single lock for pwospf subsystem -- */
    pthread_t thread;
    pthread_mutex_t lock;
};

extern struct pwospf_router *topology;

int pwospf_init(struct sr_instance* sr);
void pwospf_build_ospf_hdr(struct ospfv2_hdr *ptr, struct sr_instance* sr);
void pwospf_send_hello(struct sr_instance* sr);
void print_topology_structs();

#endif /* SR_PWOSPF_H */
