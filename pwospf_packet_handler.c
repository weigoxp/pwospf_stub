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
	printf("收到有效的OSPF！来自%s\n", inet_ntoa(ip_temp));


}