#include "pwospf_packet_handler.h"

struct q_node *q = NULL;

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
		handle_pwospf_lsu(packet, interface,sr,len);

	}


}

void enqueue(uint32_t rid, uint32_t nexthop, char *interface)
{
	struct q_node *newNode = malloc(sizeof(struct q_node));
	newNode->rid = rid;
	newNode->nexthop = nexthop;
	strcpy(newNode->interface, interface);
	newNode->next = NULL;
	if(q == NULL)
		q = newNode;
	else{
		struct q_node *ptr = q;
		while(ptr->next)
			ptr = ptr->next;
		// now ptr->next is null
		ptr->next = newNode;
	}
}

void dequeue(struct q_node* result){
	struct q_node *first = NULL;
	first = q;
	q = q->next;
	memcpy(result, first, sizeof(struct q_node));
	free(first);
}

void free_q()
{
   struct q_node* tmp;
   while (q != NULL)
    {
       tmp = q;
       q = q->next;
       free(tmp);
    }
}


void set_default_route(struct sr_instance* sr)
{
	// if we are not vhost1, then need to set default route
	if(vhost1 != 1){
		struct pwospf_interface *ifs = topology->ifs;
		while(ifs){
			if(ifs->neighbor_ip_addr != 0 && ifs->neighbor_ip_addr != -1){
				struct sr_rt *newNode = malloc(sizeof(struct sr_rt));
				newNode->dest.s_addr = 0;
				newNode->gw.s_addr = ifs->neighbor_ip_addr;
				newNode->mask.s_addr = 0;
				strcpy(newNode->interface, ifs->name);
				newNode->next = NULL;
				sr->routing_table = newNode;
				break;
			}
			ifs = ifs->next;
		}
	}
}

void empty_rt(struct sr_instance *sr)
{
	// struct sr_rt *rt = sr->routing_table;
	// struct sr_rt *tmp;
	// if(vhost1 == 1){
	// 	rt = rt->next;
	// }
	// while(rt){
	// 	tmp = rt;
	// 	rt = rt->next;
	// 	free(tmp);
	// }
	if(vhost1 == 1){
		sr->routing_table->next = NULL;
	}
	else{
		sr->routing_table = NULL;
	}
}

void print_q(){
	if(q == NULL){
		printf("Queue is empty.\n");
		return;
	}
	printf("QUEUE: \n");
	struct q_node *ptr;
	ptr = q;
	while(ptr){
		struct in_addr ip_temp = {.s_addr = ptr->rid};
		struct in_addr ip_temp2 = {.s_addr = ptr->nexthop};
		printf("%s ", inet_ntoa(ip_temp));
		printf("%s %s\n", inet_ntoa(ip_temp2), ptr->interface);
		ptr = ptr->next;
	}
}


/*
 *----------------------------------------------------------------------
 *
 * update_routing_table --
 *
 *  This function is called after we get LSU.
 *
 * Parameters:
 *      
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void update_routing_table(struct sr_instance* sr)
{
	int index = 0;	// for adding to THE array
	uint32_t added_rid[3]; 	// this array is for the recording of routers been added to the queue
	// printf("Routing table before FREE\n");
	// sr_print_routing_table(sr);
	empty_rt(sr);
	// printf("Routing table after FREE\n");
	// sr_print_routing_table(sr);
	set_default_route(sr);
	struct pwospf_interface *my_ifs = topology->ifs;
	// skip the first if exist. e.g. vhost1
	if(vhost1 == 1){
		my_ifs = my_ifs->next;
	}

	added_rid[index] = topology->rid;	// add my rid to the array.
	index++;
	// this loop go through current router's multiple interfaces.
	// finds the way out, need to store the next hop and interface name for iterating along the path.
	while(my_ifs){
		// if the connection is lost, skip. Here check for the FFF address. 
		// create new rt node:
		struct sr_rt *new_rt = malloc(sizeof(struct sr_rt));
		new_rt->dest.s_addr = my_ifs->ip_addr;
		strcpy(new_rt->interface, my_ifs->name);
		new_rt->next = NULL;
		//if broken router, set mask to 255 so dont deliver to subnet
		// and set nexthop to self
		if(my_ifs->neighbor_rid == -1){
			new_rt->gw.s_addr = 0;
			new_rt->mask.s_addr = 0xFFFFFFFF;
		}
		else { // server or connected router

			new_rt->gw.s_addr = my_ifs->neighbor_ip_addr;
			new_rt->mask.s_addr = my_ifs->mask;
		}

		// ⬆
		// add the node to rt. 
		struct sr_rt *ptr = NULL;
		ptr = sr->routing_table;

		while(ptr->next)
			ptr = ptr->next;

		ptr->next = new_rt;
		// ⬆
		if(my_ifs->neighbor_rid != -1 && my_ifs->neighbor_rid != 0){
			enqueue(my_ifs->neighbor_rid, my_ifs->neighbor_ip_addr, my_ifs->name);
			added_rid[index] = my_ifs->neighbor_rid;
			index++;
		}
		my_ifs = my_ifs->next;
	}
	// now the subnets directly connected to me have been checked. 
	print_q();
	while(q){
		struct q_node *current = malloc(sizeof(struct q_node));
		dequeue(current);
		printf("After DEQUEUE: \n");
		print_q();
		struct pwospf_router *router = NULL;
		router = get_router_with_rid(current->rid);
		// in the process of convergence, a router could have 2 topology node. 
		if(router == NULL){
			continue;
		}
		struct pwospf_interface *ifs = router->ifs;
		// iterate the interface list in the current router from queue
		while(ifs){

			struct sr_rt *newNode = malloc(sizeof(struct sr_rt));
			newNode->dest.s_addr = ifs->ip_addr;
			newNode->gw.s_addr = current->nexthop;
			if(ifs->neighbor_rid == -1)
				newNode->mask.s_addr = 0xFFFFFFFF;
			
			else
				newNode->mask.s_addr = ifs->mask;	
			
			strcpy(newNode->interface, current->interface);
			newNode->next = NULL;
			// now add the node to rt
			struct sr_rt *ptrr = NULL;
			ptrr = sr->routing_table;
			while(ptrr->next)
				ptrr = ptrr->next;
			ptrr->next = newNode;
			// 对于每一个已连接的interface来说，不在array里的有效RID要加入queue
			if(ifs->neighbor_rid != 0 && ifs->neighbor_rid != -1){
				int i = 0;
				int flag = 1; // 0 means already exist no need to add, 1 means do not exist. 
				// iterate through the array
				for (i = 0; i < 3; ++i){
					if(added_rid[i] == ifs->neighbor_rid){
						flag = 0;
						break;
					}
				}
				// here, if we have not ever see the matched RID, then add. 
				if(flag == 1){
					enqueue(ifs->neighbor_rid, current->nexthop, current->interface);
					added_rid[index] = ifs->neighbor_rid;
					index++;
				}
			}
			ifs = ifs->next;
		}
		free(current);
	}
	free_q();
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
//----------------------------------------------------------------------


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

//----------------------------------------------------------------------

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
	routers->next->ifs = NULL;
	routers->next->next = NULL;
	return routers->next;
}
//----------------------------------------------------------------------
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

void forward_lsu(struct sr_instance* sr, uint8_t * packet,unsigned int len, char* interface)
{
	struct sr_ethernet_hdr *e_hdr = (struct sr_ethernet_hdr *) packet;
	struct sr_if *ifs = sr->if_list;
	while(ifs){
		int flag=1;
		struct pwospf_interface *curifs = topology->ifs;
		while(curifs){
			//find cur ether if in topo if.
			if(strcmp(curifs->name, ifs->name)==0){
				if(curifs->neighbor_rid ==0 ||curifs->neighbor_rid ==-1)
					flag =0;
			}
			curifs=curifs->next;
		}

		// dont forward the packet to where it came from. or broke router or server
		if(strcmp(interface, ifs->name) == 0 || flag==0){
			ifs = ifs->next;
			continue;
		}
		memcpy(e_hdr->ether_shost, ifs->addr, ETHER_ADDR_LEN);
		sr_send_packet(sr, packet,len,ifs->name);
		printf("已转发LSU\n");
		ifs = ifs->next;
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
void handle_pwospf_lsu(uint8_t * packet, char* interface, struct sr_instance* sr,unsigned int len)
{
	// We surely need to know which router sends this packet, identify this from OSPF header.
	struct ospfv2_hdr *ospf_hdr = (struct ospfv2_hdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
	struct ospfv2_lsu_hdr *lsu_hdr = (struct ospfv2_lsu_hdr *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr));
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

	if(result_router != NULL){
			// check if the lsu packet's seq is newer. if not drop.
		if(lsu_hdr->seq <= result_router->last_seq){
			printf("the LSU is too old, drop\n");
			return;
		}
	}

	else{
		// if does not exist, we create one. 
		result_router = create_router_node(ospf_hdr->rid);

	}



	result_router->last_seq = lsu_hdr->seq;
	// if we find exsiting router in the topology structure. 
	free_ifs(result_router->ifs);
	// the first one: 
	result_router->ifs = malloc(sizeof(struct pwospf_interface));
	struct pwospf_interface *ifs = result_router->ifs;
	struct ospfv2_lsu *lsu = (struct ospfv2_lsu *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr));
	strcpy(ifs->name, "Unknown");
	ifs->ip_addr = (lsu->subnet);
	ifs->mask = (lsu->mask);
	ifs->helloint = OSPF_DEFAULT_HELLOINT;
	ifs->neighbor_rid = (lsu->rid);
	ifs->neighbor_ip_addr = 0;
	ifs->ts = 0;
	ifs->next = NULL;
	// now the rest of advertisements.

	printf("!@%d\n", (lsu_hdr->num_adv));
	int i; // initially 1 for skipping one above.
	for (i = 1; i < (lsu_hdr->num_adv); ++i)
	{
		printf("MORE LSU\n");
		ifs->next = malloc(sizeof(struct pwospf_interface));
		ifs = ifs->next;
		lsu = (struct ospfv2_lsu *) (packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr) + i * sizeof(struct ospfv2_lsu));
		strcpy(ifs->name, "Unknown");
		ifs->ip_addr = (lsu->subnet);
		ifs->mask = (lsu->mask);
		ifs->helloint = OSPF_DEFAULT_HELLOINT;
		ifs->neighbor_rid = (lsu->rid);
		ifs->neighbor_ip_addr = 0;
		ifs->ts = 0;
		ifs->next = NULL;
	}
		

		// report topology updated.
	printf("!!! UPDATED topology:\n");
	print_topology_structs();
	update_routing_table(sr);
	printf("!?! UPDATED routing table\n");
	sr_print_routing_table(sr);

	// forward the LSU to all interfaces except the one it came from.
	forward_lsu(sr, packet, len, interface);

}







