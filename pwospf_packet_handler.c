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
	}
	if (ospf_hdr->type == OSPF_TYPE_LSU)
	{
		printf("收到有效的LSU！来自%s\n", inet_ntoa(ip_temp));
		handle_pwospf_lsu();
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
 *      sr: The instance of current router
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void handle_pwospf_hello(uint8_t * packet, char* interface)
{
	// // need neighbor ip and rid. both uint32_t.
	// struct ip *ip_hdr = (struct ip *) (packet + sizeof(struct sr_ethernet_hdr));
	// struct pwospf_interface *current_ifs = topology->ifs;
	// while(current_ifs)
	// {
	// 	if(strcmp(current_ifs->name, interface))
	// 	{
	// 		// found the correct interface to fill the info in hello. 
	// 		return;
	// 	}
	// 	current_ifs = current_ifs->next; 
	// }
	// printf("Unrecognized interface %s\n", interface);

}



/*
 *----------------------------------------------------------------------
 *
 * handle_pwospf_lsu --
 *
 * This function ...  
 *
 * Parameters:
 *      
 *  
 * Return: None.
 *
 *----------------------------------------------------------------------
 */
void handle_pwospf_lsu()
{

}