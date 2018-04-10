#include <linux/if_ether.h> /* ETH_P_ARP, ETH_ALEN, struct ethhdr */


/*
  struct arphdr from linux/if_arp.h currently doesn't include these members for
  the rest of the request so we must create it ourselves. These are copied
  from linux/if_arp.h
*/
struct arpbdy {
  unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
  unsigned char		ar_sip[4];		/* sender IP address		*/
  unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
  unsigned char		ar_tip[4];		/* target IP address		*/

};


#define ARP_HLEN sizeof(struct arphdr)
#define ARP_BLEN sizeof(struct arpbdy)
#define USAGE "./arpsend [-t request | reply] [--link-layer] source_mac source_ip dest_mac dest_ip"
