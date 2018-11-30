#include "pwospf_packet_handler.h"


void handle_pwospf_packet(	struct sr_instance* sr,
							uint8_t * packet/* lent */,
							unsigned int len,
                			char* interface)
{
	struct ospfv2_hdr *ospf_hdr = (struct ospfv2_hdr *) (packet + (sizeof(struct ip)) + (sizeof(struct sr_ethernet_hdr)));
	// check for the OSPF version, must be 2
	if(ospf_hdr->version != OSPF_V2)
		return;
	// check for the area ID, must be the same as the router
	if(ospf_hdr->aid != AREA_ID_IN_THIS_PROJECT)
		return;
	// check for the authentication type
	if(ospf_hdr->autype != AU_TYPE_IN_THIS_PROJECT)
		return;
	struct in_addr ip_temp = {.s_addr = ospf_hdr->rid};


	if (ospf_hdr->type == OSPF_TYPE_HELLO)
	{
		printf("收到有效的HELLO！来自%s\n", inet_ntoa(ip_temp));
		handle_pwospf_hello(packet, interface);
		// report topology updated.
		printf("@@@ UPDATED topology:\n");
		print_topology_structs();
	}
	if (ospf_hdr->type == OSPF_TYPE_LSU)
	{
		printf("收到有效的LSU！来自%s\n", inet_ntoa(ip_temp));
		handle_pwospf_lsu(packet, interface);
		// report topology updated.
		printf("!!! UPDATED topology:\n");
		print_topology_structs();
	}


}

/*
 *----------------------------------------------------------------------
 *
 * handle_pwospf_hello --
 *
 *  This function is called if packet handler recognizes the packet is a hello message
 *	Need to take the information in the packet, like RID and the recieving interfaces, 
 *	find them in the topology, update them. 
 *	Note: The hello message can only modify current router's neighbors, not other routers and their interfaces. 
 *
 * Parameters:
 *      packet: The whole packet including Ethernet and IP header. 
 * 		interface: specifies which interface the packet comes from. 
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void handle_pwospf_hello(uint8_t * packet, char* interface)
{
	// need neighbor ip and rid. both uint32_t.
	struct ip *ip_hdr = (struct ip *) (packet + sizeof(struct sr_ethernet_hdr));
	struct ospfv2_hdr *ospf_hdr = (struct ospfv2_hdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
	struct pwospf_interface *current_ifs = topology->ifs;
	while(current_ifs)
	{
		if(strcmp(current_ifs->name, interface) == 0)
		{
			// found the correct interface to fill the rid and ip from hello. 
			current_ifs->neighbor_rid = ospf_hdr->rid;
			current_ifs->neighbor_ip_addr = ip_hdr->ip_src.s_addr;
			current_ifs->ts = time(NULL);
			return;
		}
		current_ifs = current_ifs->next; 
	}
	printf("Unrecognized interface %s\n", interface);
}

struct pwospf_router * get_router_with_rid(uint32_t rid)
{
	struct pwospf_router *routers = topology;
	while(routers)
	{
		if(routers->rid == rid)
		{
			return routers;
		}
		routers = routers->next;
	}
	return NULL;
}

struct pwospf_router * create_router_node(uint32_t rid)
{
	struct pwospf_router *routers = topology;
	while(routers->next){
		routers = routers->next;
	}
	routers->next = malloc(sizeof(struct pwospf_router));
	routers->next->rid = rid;
	routers->next->aid = AREA_ID_IN_THIS_PROJECT;
	routers->next->lsuint = OSPF_DEFAULT_LSUINT;
	routers->next->last_seq = 0;
	return routers->next;
}

void free_ifs(struct pwospf_interface* head)
{
   struct pwospf_interface* tmp;
   while (head != NULL)
    {
       tmp = head;
       head = head->next;
       free(tmp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * handle_pwospf_lsu --
 *
 * This function is called if the ospf packet handler recognizes the packet is a link state update message
 * The information is about routers in the whole topology, not necessarily our neighbors. 
 * After update the topology data structure, we need to forward the LSU to all of our interfaces except the one that we receive this LSU.
 *
 * Note: The LSU message *SHOULD* modify the router list, add more if a new router showed up in the update. 
 *
 * Parameters:
 *      packet: The whole packet including Ethernet and IP header. 
 * 		interface: specifies which interface the packet comes from. (Need *NOT* to forward to this interface)
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void handle_pwospf_lsu(uint8_t * packet, char* interface)
{
	// We surely need to know which router sends this packet, identify this from OSPF header.
	struct ospfv2_hdr *ospf_hdr = (struct ospfv2_hdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
	struct ospfv2_lsu_hdr *lsu_hdr = (struct ospfv2_lsu_hdr *) (ospf_hdr + sizeof(struct ospfv2_hdr));
	// need to loop through router list to find this one. 
	struct pwospf_router *routers = topology;
	// first check if the packet is from current router:
	if(routers->rid == ospf_hdr->rid){
		// the LSU is *FROM* us, drop
		printf("the LSU is *FROM* us, drop\n");
		return;
	}

	// then check the whole topology.
	struct pwospf_router *result_router = get_router_with_rid(ospf_hdr->rid);
	if(result_router == NULL){
		// if does not exist, we create one. 
		result_router = create_router_node(ospf_hdr->rid);
	}
	// if we find exsiting router in the topology structure. 
	free_ifs(result_router->ifs);
	// the first one: 
	result_router->ifs = malloc(sizeof(struct pwospf_interface));
	struct pwospf_interface *ifs = result_router->ifs;
	struct ospfv2_lsu *lsu = (struct ospfv2_lsu *) (lsu_hdr + sizeof(struct ospfv2_lsu_hdr));
	strcpy(ifs->name, "Unknown");
	ifs->ip_addr = (lsu->subnet);
	ifs->mask = (lsu->mask);
	ifs->helloint = OSPF_DEFAULT_HELLOINT;
	ifs->neighbor_rid = (lsu->rid);
	ifs->neighbor_ip_addr = 0;
	ifs->ts = 0;
	// now the rest of advertisements.

	printf("!@%d\n", (lsu_hdr->num_adv));
	int i; // initially 1 for skipping one above.
	for (i = 1; i < ntohl(lsu_hdr->num_adv); ++i)
	{
		printf("MORE LSU\n");
		ifs->next = malloc(sizeof(struct pwospf_interface));
		ifs = ifs->next;
		lsu += sizeof(struct ospfv2_lsu);
		strcpy(ifs->name, "Unknown");
		ifs->ip_addr = ntohl(lsu->subnet);
		ifs->mask = ntohl(lsu->mask);
		ifs->helloint = OSPF_DEFAULT_HELLOINT;
		ifs->neighbor_rid = ntohl(lsu->rid);
		ifs->neighbor_ip_addr = 0;
		ifs->ts = 0;
	}

}







