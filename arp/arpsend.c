#include <net/if.h> /* struct ifreq, if_nametoindex */
#include <sys/socket.h> /* PF_PACKET, SOCK_RAW */
#include <linux/if_ether.h> /* ETH_P_ARP, ETH_ALEN, struct ethhdr */
#include <linux/if_packet.h> /* struct sockaddr_ll */
#include <linux/if_arp.h> /*ARPOP_REQUEST, ARPOP_REPLY, ARPHRD_ETHER, struct arphdr */
#include <arpa/inet.h> /* htons */
#include <sys/ioctl.h> /* ioctl, SIOCGIFHWADDR */

#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARP_HLEN 8
#define ARP_BLEN 20
#define USAGE "./arpsend [-t request | reply] [--link-layer] source_mac source_ip dest_mac dest_ip"

/*
  struct arphdr from linux/if_arp.h doesn't include members for the rest of the request
  so we must create it ourselves
*/
struct arpbdy {
  unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
  unsigned char		ar_sip[4];		/* sender IP address		*/
  unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
  unsigned char		ar_tip[4];		/* target IP address		*/

};

int main(int argc, char *argv[argc])
{
  int socket_fd;
  unsigned int iface_index;
  struct ifreq iface;
  struct sockaddr_ll opts;
  struct ethhdr ethernet_header;
  struct arphdr arp_header;
  struct arpbdy arp_body;
  struct in_addr ipv4_addr;
  uint8_t buffer[ETH_HLEN+ARP_HLEN+ARP_BLEN];

  // Create socket
  socket_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
  if (socket_fd == -1) {
    perror("Failed to open socket");
    exit(1);
  }
  fprintf(stderr, "(DEBUG) Successfully opened raw socket: %i\n", socket_fd);

  // Get interface index
  iface_index = if_nametoindex("wlan0");
  if (!iface_index) {
    fprintf(stderr, "Failed to obtain %s interface index: %s\n", "wlan0", strerror(errno));
    exit(1);
  }
  fprintf(stderr, "(DEBUG) Successfully obtained %s interface index: %i\n", "wlan0", iface_index);

  // Get mac address of interface
  strcpy(iface.ifr_name, "wlan0");
  if (ioctl(socket_fd, SIOCGIFHWADDR, &iface) == -1) {
    fprintf(stderr, "Failed to obtain mac address of %s interface: %s", iface.ifr_name, strerror(errno));
    exit(1);
  }
  fprintf(stderr, "(DEBUG) Successfully obtained mac address of %s interface: ", iface.ifr_name);
  for (int i = 0; i < 5; i++)
    fprintf(stderr, "%2x:", (unsigned char) iface.ifr_hwaddr.sa_data[i]);
  fprintf(stderr, "%.2x\n", (unsigned char) iface.ifr_hwaddr.sa_data[5]);

  
  // Add options
  opts.sll_family = AF_PACKET;
  opts.sll_protocol = htons(ETH_P_ARP);
  opts.sll_ifindex = iface_index;
  opts.sll_hatype = ARPHRD_ETHER;
  opts.sll_halen = ETH_ALEN;
  memcpy(opts.sll_addr, iface.ifr_hwaddr.sa_data, ETH_ALEN);

  // Create link layer header
  unsigned char foo[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  unsigned char bar[] = {0x82, 0x2d, 0xe7, 0x9d, 0x93, 0xbb}; /* NOT NECESSARY TO USE ACTUAL MAC ADDRESS OF INTERFACE */
  memcpy(ethernet_header.h_dest, foo, ETH_ALEN);
  /* memcpy(ethernet_header.h_source, iface.ifr_hwaddr.sa_data, ETH_ALEN); */
  memcpy(ethernet_header.h_source, bar, ETH_ALEN);
  ethernet_header.h_proto = htons(ETH_P_ARP);

  // Create arp request header
  arp_header.ar_hrd = htons(ARPHRD_ETHER);
  arp_header.ar_pro = htons(0x0800);
  arp_header.ar_hln = ETH_ALEN;
  arp_header.ar_pln = 4;
  arp_header.ar_op = htons(ARPOP_REQUEST);

  // Create arp request body
  memcpy(arp_body.ar_sha, bar, ETH_ALEN);
  inet_aton("192.168.99.67", &ipv4_addr);
  for (int i = 0; i < 4; i++)
    arp_body.ar_sip[i] = *((char *) (&ipv4_addr.s_addr)+i);
  memcpy(arp_body.ar_tha, foo, ETH_ALEN);
  inet_aton("192.168.99.3", &ipv4_addr);
  for (int i = 0; i < 4; i++)
    arp_body.ar_tip[i] = *((char *) (&ipv4_addr.s_addr)+i);
  
  // Copy ethernet header, arp header, and arp body into buffer
  memcpy(buffer, &ethernet_header, ETH_HLEN);
  memcpy(buffer+ETH_HLEN, &arp_header, ARP_HLEN);
  memcpy(buffer+ETH_HLEN+ARP_HLEN, &arp_body, ARP_BLEN);

  int i = 0;
  printf("0x%04x:\t", i);  
  while (i < ETH_HLEN+ARP_HLEN+ARP_BLEN) {
    printf("%02x%02x ", buffer[i], buffer[i+1]);
    i+=2;
    if (!(i%16)) {
      printf("\n");
      printf("0x%04x:\t", i);
    }
  }
  printf("\n");

  printf("Sending arp request...\n");

  int bytes_sent;
  if ((bytes_sent = sendto(socket_fd,
			   buffer,
			   ETH_HLEN+ARP_HLEN+ARP_BLEN,
			   0,
			   (struct sockaddr *)&opts,
			   sizeof(opts))) == -1) {
    perror("Failed to send arp request");
    exit(1);
  }

  printf("Arp request sent!\n");
  printf("%i\n", bytes_sent == ETH_HLEN+ARP_HLEN+ARP_BLEN);
  
  close(socket_fd);
  return 0;
}
