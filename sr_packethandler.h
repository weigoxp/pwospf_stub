#ifndef SR_PACKETHANDLER_H
#define SR_PACKETHANDLER_H

#ifndef PWOSPF_IP_PROTOCOL
#define PWOSPF_IP_PROTOCOL 89
#endif

struct buffer_node
{
    uint8_t * packet;
    unsigned int len;
    uint32_t nexthop;
    struct buffer_node *next;
};

struct sr_icmp_hdr
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
};

void handleArp( struct sr_arphdr *,
				struct sr_instance* ,
				struct sr_ethernet_hdr *,
				uint8_t * /* lent */,
				unsigned int len,
				char* /* lent */);


void handleIp(  struct ip* ,
                struct sr_instance* ,
                struct sr_ethernet_hdr *,
                uint8_t * /* lent */,
                unsigned int ,
                char* interface);

void testarp(uint8_t *packet);
void testip(struct ip* );

u_short checksum(u_short  *buf, int count);

uint32_t sr_getInterfaceAndNexthop(struct sr_instance *sr, 
                                uint32_t iphdrDest,
                                char * interface);


void sendArpRequest(
                    struct sr_instance* sr,
                    uint32_t nexthopIP,//pass in final ip in ip header 
                    char* interface // going out inferface
                        );
uint32_t findAddrsForInterface(struct sr_instance *sr, char *interface, unsigned char *addr);
void send_icmp_echo_reply(  struct sr_instance* sr, 
                            uint8_t * packet, 
                            unsigned int len, 
                            struct ip* iphdr, 
                            char* interface);

#endif