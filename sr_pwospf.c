/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 * date: Tue Nov 23 23:24:18 PST 2004
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sr_pwospf.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_packethandler.h"
#include "pwospf_packet_handler.h"



/* -- declaration of main thread function for pwospf subsystem --- */
static void* pwospf_run_thread(void* arg);

// The topology structure, the first node in the list is current router by default. 
struct pwospf_router *topology = NULL;

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Sets up the internal data structures for the pwospf subsystem
 *
 * You may assume that the interfaces have been created and initialized
 * by this point.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance* sr)
{
    assert(sr);

    sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct
                                                      pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);


    /* -- handle subsystem initialization here! -- */
    // ------------- initialization of topology structs -------------⬇ 
    topology = malloc(sizeof(struct pwospf_router));
    topology->rid = sr->if_list->ip;
    topology->aid = AREA_ID_IN_THIS_PROJECT;
    topology->lsuint = OSPF_DEFAULT_LSUINT;


    topology->ifs = (struct pwospf_interface *) malloc(sizeof(struct pwospf_interface));
    struct pwospf_interface *topology_ifs = topology->ifs;
    struct sr_if *interfaces = sr->if_list;
    while(interfaces->next)
    {
        topology_ifs->next = (struct pwospf_interface *) malloc(sizeof(struct pwospf_interface));
        topology_ifs = topology_ifs->next;
        interfaces = interfaces->next;
    }
    interfaces = sr->if_list;
    topology_ifs = topology->ifs;

    // // TODO: here checks the topology interfaces has 4 instances.
    // while(topology_ifs)
    // {
    //     printf("0\n");
    //     topology_ifs = topology_ifs->next;
    // }
    // topology_ifs = topology->ifs;


    while(interfaces)
    {
        //printf("%p\n", interfaces);
        strcpy(topology_ifs->name, interfaces->name);
        topology_ifs->ip_addr = interfaces->ip;
        topology_ifs->mask = interfaces->mask;
        topology_ifs->helloint = OSPF_DEFAULT_HELLOINT;

        topology_ifs = topology_ifs->next;
        interfaces = interfaces->next;
    }
    // ------------- initialization of topology structs -------------⬆

    // TESTING initial topology structures. 
    print_topology_structs();


    /* -- start thread subsystem -- */
    if( pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr)) {

        perror("pthread_create");
        assert(0);
    }

    return 0; /* success */
} /* -- pwospf_init -- */


/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Main thread of pwospf subsystem.
 *
 *---------------------------------------------------------------------*/

static
void* pwospf_run_thread(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;
    int LSU_count = 0;
    while(1)
    {
        /* -- PWOSPF subsystem functionality should start  here! -- */

        pwospf_lock(sr->ospf_subsys);

        pwospf_send_hello(sr);
       
        if(LSU_count % 6 ==0)
            pwospf_send_LSU(sr); 
        LSU_count ++; 
        pwospf_unlock(sr->ospf_subsys);
        printf(" pwospf subsystem sleeping for 5 secs\n");
        // sleep for 5 secs
        sleep(OSPF_DEFAULT_HELLOINT); 

        printf(" pwospf subsystem awake \n");
    };
} /* -- run_ospf_thread -- */

/*
 *----------------------------------------------------------------------
 *
 * pwospf_build_ospf_hdr --
 *
 *  This function fills in the passed pointer ptr with a OSPFv2 header.
 *
 * Parameters:
 *      ptr: The pointer to beginning of OSPF header.
 *      sr: The instance of current router
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void pwospf_build_ospf_hdr(struct ospfv2_hdr *ptr, struct sr_instance* sr, uint8_t OSPF_TYPE)
{
    // set the version to be v2
    ptr->version = OSPF_V2;
    // set the OSPF type to be hello(1)
    ptr->type = OSPF_TYPE;
    ptr->len = sizeof(ptr);
    // set source router ID
    ptr->rid = sr->if_list->ip;
    // the following fields are set to 0 according to spec convention.
    ptr->aid = AREA_ID_IN_THIS_PROJECT;
    ptr->autype = AU_TYPE_IN_THIS_PROJECT;
    ptr->audata = AU_DATA_IN_THIS_PROJECT;
    ptr->csum = (uint16_t) checksum((u_short *)ptr, sizeof(struct ospfv2_hdr) / 2);
}

/*
 *----------------------------------------------------------------------
 *
 * pwospf_send_hello --
 *
 *  This function send a hello message to all the neighbors ONCE. 
 *
 * Parameters:
 *      sr: The instance of current router
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void pwospf_send_hello(struct sr_instance* sr)
{
    struct sr_if *interfaces = sr->if_list;
    // iterate through all the interfaces, send 1 hello through each interface.
    while(interfaces)
    {

        // the packet include a Ethernet header, IP header, OSPF header and OSPF hello header.
        void *packet = malloc(sizeof(struct sr_ethernet_hdr) + 
                                sizeof(struct ip) + sizeof(struct ospfv2_hdr) + 
                                sizeof(struct ospfv2_hello_hdr));
        // Ethernet header filling is done inside the function below: 
        struct sr_ethernet_hdr *e_hdr = (struct sr_ethernet_hdr *) packet;
        memset(e_hdr->ether_dhost, 0xFF, 6);
        memcpy(e_hdr->ether_shost, interfaces->addr, 6);
        e_hdr->ether_type = htons(ETHERTYPE_IP);
        // DONE with Ethernet header

        // fill IP header
        struct ip *ip_hdr = packet + sizeof(struct sr_ethernet_hdr);
        ip_hdr->ip_v = 4;  
        ip_hdr->ip_hl = 5; // length = 5* 4 =20
        ip_hdr->ip_tos = 0;
        ip_hdr->ip_len = htons(sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr));
        ip_hdr->ip_id = 0;
        ip_hdr->ip_off = 0;
        ip_hdr->ip_ttl = 3;
        ip_hdr->ip_p = 89;  //ospf identifier
        ip_hdr->ip_src.s_addr = interfaces->ip;
        ip_hdr->ip_dst.s_addr = OSPF_AllSPFRouters;
        u_short chksum;
        chksum = checksum((u_short *)ip_hdr, ip_hdr->ip_hl*2);
        ip_hdr->ip_sum = htons(chksum);
        // DONE with IP header

        // allocate the memroy for ospf header and hello header, that's all we need for a hello packet.
        struct ospfv2_hdr *ospf_hdr = packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
        pwospf_build_ospf_hdr(ospf_hdr, sr, OSPF_TYPE_HELLO);

        // fill in hello header
        struct ospfv2_hello_hdr *hello_hdr = packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr);
        hello_hdr->nmask = interfaces->mask;
        hello_hdr->helloint = OSPF_DEFAULT_HELLOINT;

        // ready to send packet. 
        sr_send_packet(sr, packet, 
                        sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)+
                        sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr)
                            , interfaces->name);
        interfaces = interfaces->next;
    }
    printf("已给所有interface发Hello。\n");

}

// void pwospf_send_LSU(struct sr_instance* sr)
// {
//     printf("(假装)已给所有neighbor发LSU。\n");
// }
/*
 *----------------------------------------------------------------------
 *
 * pwospf_send_LSU
 *
 *  This function send a LSU message to all the neighbors ONCE. 
 *
 * Parameters:
 *      sr: The instance of current router
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */



 void pwospf_send_LSU(struct sr_instance* sr )
{
   struct sr_if *interfaces = sr->if_list;

    while(interfaces)
    {

        // the packet include a Ethernet header, IP header, OSPF header and OSPF LSU header.
        void *packet = malloc(sizeof(struct sr_ethernet_hdr) + 
                                sizeof(struct ip) + sizeof(struct ospfv2_hdr) + 
                                sizeof(struct ospfv2_lsu_hdr)+
                                3*sizeof(struct ospfv2_lsu));

        // Ethernet header filling is done inside the function below: 
        struct sr_ethernet_hdr *e_hdr = (struct sr_ethernet_hdr *) packet;
        memset(e_hdr->ether_dhost, 0xFF, 6);
        memcpy(e_hdr->ether_shost, interfaces->addr, 6);
        e_hdr->ether_type = htons(ETHERTYPE_IP);
        // DONE with Ethernet header

        // Now IP.
        struct ip *ip_hdr = packet + sizeof(struct sr_ethernet_hdr);
        ip_hdr->ip_v = 4;
        ip_hdr->ip_hl = 5;
        ip_hdr->ip_tos = 0;
        ip_hdr->ip_len = htons(sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr));
        ip_hdr->ip_id = 0;
        ip_hdr->ip_off = 0;
        ip_hdr->ip_ttl = 3;
        ip_hdr->ip_p = 89;
        ip_hdr->ip_src.s_addr = interfaces->ip;
        ip_hdr->ip_dst.s_addr = OSPF_AllSPFRouters;
        u_short chksum;
        chksum = checksum((u_short *)ip_hdr, ip_hdr->ip_hl*2);
        ip_hdr->ip_sum = htons(chksum);
        // DONE with IP header

        // allocate the memroy for ospf header and lsu header
        struct ospfv2_hdr *ospf_hdr = packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
        pwospf_build_ospf_hdr(ospf_hdr, sr, OSPF_TYPE_LSU);


        // fill in lsu header
        struct ospfv2_lsu_hdr *lsu_hdr = packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr);

        //lsu_hdr->seq = topology->seq ;

        lsu_hdr->ttl = OSPF_MAX_LSU_TTL; // 255
        lsu_hdr->num_adv = (3); //We hardcode it to 3 here. 

        // fill in lsu advertisement
        int i =0;
        //topology->ifs  first router, which is 
        struct pwospf_interface *pwospf_ifs = topology->ifs;

        while(pwospf_ifs){
            struct ospfv2_lsu *lsu = (struct ospfv2_lsu *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr) + i * sizeof(struct ospfv2_lsu));
            lsu ->subnet = pwospf_ifs->ip_addr;
            lsu -> mask = pwospf_ifs->mask;
            lsu -> rid = pwospf_ifs->neighbor_rid;
            pwospf_ifs= pwospf_ifs->next;
            i++;
        }

        // ----------
        struct ospfv2_lsu *t = packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr) + sizeof(struct ospfv2_lsu);
        struct in_addr tt = {.s_addr = t->rid};
        printf("~~~~~~~~~%s\n", inet_ntoa(tt));

        // ----------
        // ready to send packet. 
        sr_send_packet(sr, packet,sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)+
                                    sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr)
                                    +3*sizeof(struct ospfv2_lsu), 
                                    interfaces->name);

        interfaces = interfaces->next;
    }
    printf("已给所有neighbor发LSU。\n");
}



void print_topology_structs()
{
    struct pwospf_router *pointer = topology;
    int n = 0;
    
    while(pointer)
    {
        printf("================ Router %d ================\n", n);
        struct in_addr ip_temp = {.s_addr = pointer->rid};
        printf("\tRouter ID: %s\n", inet_ntoa(ip_temp));
        printf("\tArea ID: %d\n", pointer->aid);
        printf("\tLSU int: %d\n", pointer->lsuint);
        
        struct pwospf_interface *interface_p = pointer->ifs;
        while(interface_p)
        {
            printf("\t---------- Interface ----------\n");
            printf("\tName: %s\n", interface_p->name);
            ip_temp.s_addr = interface_p->ip_addr;
            printf("\tIP address: %s\n", inet_ntoa(ip_temp));
            ip_temp.s_addr = interface_p->mask;
            printf("\tMask: %s\n", inet_ntoa(ip_temp));
            printf("\tHello int: %d\n", interface_p->helloint);
            ip_temp.s_addr = interface_p->neighbor_rid;
            printf("\tNeighbor RID: %s\n", inet_ntoa(ip_temp));
            ip_temp.s_addr = interface_p->neighbor_ip_addr;
            printf("\tNeighbor IP address: %s\n", inet_ntoa(ip_temp));
            char *t = ctime(&(interface_p->ts));
            printf("\tTime Stamp: %s\n", t);
            interface_p = interface_p->next;
            printf("\t-------------------------------\n");
        }
        pointer = pointer->next;
        n++;
        printf("==========================================\n");
    }
}







