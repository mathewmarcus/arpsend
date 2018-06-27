#include <net/if.h> /* struct ifreq, if_nametoindex */
#include <sys/socket.h> /* PF_PACKET, SOCK_RAW, AF_PACKET */
#include <linux/if_packet.h> /* struct sockaddr_ll */
#include <linux/if_arp.h> /*ARPOP_REQUEST, ARPOP_REPLY, ARPHRD_ETHER, struct arphdr */

#include <arpa/inet.h> /* htons */
#include <sys/ioctl.h> /* ioctl, SIOCGIFHWADDR */

#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "arp.h"

static unsigned char *packet_atomac(unsigned char *packet, const char *mac_address) {
  unsigned int buffer[6];

  if (sscanf(mac_address,
	     "%x:%x:%x:%x:%x:%x",
	     buffer, buffer+1, buffer+2, buffer+3, buffer+4, buffer+5) != 6) {
    fprintf(stderr, USAGE);
    exit(1);
  }
  for (int i = 0; i < 6; i++)
    packet[i] = (unsigned char) buffer[i];
  return packet;
}

static void packet_print(uint8_t *buffer, socklen_t buffer_size) {
  unsigned int i = 0;

  printf("0x%04x:\t", i);  
  while (i < buffer_size) {
    printf("%02x%02x ", buffer[i], buffer[i+1]);
    i+=2;
    if (!(i%16)) {
      printf("\n");
      printf("0x%04x:\t", i);
    }
  }
  printf("\n\n");


}

void print_arp_response(int socket_fd, socklen_t buffer_size) {
  uint8_t *buffer = calloc(buffer_size, sizeof(uint8_t));
  int recvfrom_res;

  if ((recvfrom_res = recvfrom(socket_fd,
			       buffer,
			       buffer_size,
			       0,
			       NULL,
			       NULL)) == -1) {
    perror("recvfrom error");
    free(buffer);
    exit(1);
  }
  else if (recvfrom_res == 0) {
    fprintf(stderr, "Failed to receive arp response\n");
    free(buffer);
    exit(1);
  }
  packet_print(buffer, buffer_size);
  free(buffer);
}


uint8_t *add_l2_header(uint8_t *packet, const char *dest_mac, const char *source_mac) {
  struct ethhdr ethernet_header;

  // Create link layer header
  packet_atomac(ethernet_header.h_dest, dest_mac);
  packet_atomac(ethernet_header.h_source, source_mac);
  ethernet_header.h_proto = htons(ETH_P_ARP);

  memcpy(packet, &ethernet_header, ETH_HLEN);

  return packet+=ETH_HLEN;
}

uint8_t *add_arp_header(uint8_t *packet, const char *arp_op) {
  struct arphdr arp_header;
  
  // Create arp request header
  arp_header.ar_hrd = htons(ARPHRD_ETHER);
  arp_header.ar_pro = htons(0x0800);
  arp_header.ar_hln = ETH_ALEN;
  arp_header.ar_pln = 4;

  if (!strncmp(arp_op, "request", 7))    
    arp_header.ar_op = htons(ARPOP_REQUEST);
  else if (!strncmp(arp_op, "reply", 5))
    arp_header.ar_op = htons(ARPOP_REPLY);
  else {
    fprintf(stderr, USAGE);
    exit(1);
  }

  memcpy(packet, &arp_header, ARP_HLEN);
  return packet+=ARP_HLEN;
}

uint8_t *add_arp_body(uint8_t *packet,
		      const char *source_mac,
		      const char *source_ip,
		      const char *dest_mac,
		      const char *dest_ip) {
  struct arpbdy arp_body;
  struct in_addr ipv4_addr;

  // Create arp request body
  packet_atomac(arp_body.ar_sha, source_mac);
  if (!inet_aton(source_ip, &ipv4_addr)) {
    fprintf(stderr, USAGE);
    exit(1);
  }
  for (int i = 0; i < 4; i++)
    arp_body.ar_sip[i] = *((char *) (&ipv4_addr.s_addr)+i);

  packet_atomac(arp_body.ar_tha, dest_mac);
  if (!inet_aton(dest_ip, &ipv4_addr)) {
    fprintf(stderr, USAGE);
    exit(1);
  }
  for (int i = 0; i < 4; i++)
    arp_body.ar_tip[i] = *((char *) (&ipv4_addr.s_addr)+i);
  
  // Copy ethernet header, arp header, and arp body into buffer
  memcpy(packet, &arp_body, ARP_BLEN);

  return packet+=ARP_BLEN;
}


int main(int argc, char *argv[argc])
{
  int socket_fd, c, l2_header = 1;
  unsigned int iface_index, total_length;
  struct ifreq iface;
  struct sockaddr_ll opts;
  uint8_t buffer[ETH_HLEN+ARP_HLEN+ARP_BLEN], *ptr;
  char *iface_name = NULL, *type = "request";
  static unsigned char broadcast_mac_adress[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  opterr = 0;

  while ((c = getopt(argc, argv, "t:ni:")) != -1) {
    switch (c) {
    case 'n':
      l2_header = 0;
      break;
    case 'i':
      iface_name = optarg;
      break;
    case 't':
      type = optarg;
      break;
    case '?': // Unexpected opt
    case ':': // Missing opt arg
      fprintf(stderr, USAGE);
      exit(1);
    }
    
  }

  if (!iface_name ||
      strlen(iface_name) > IFNAMSIZ ||
      argc - optind != 4) {
    fprintf(stderr, USAGE);
    exit(1);
  }

  ptr = buffer;
  if (l2_header)
    ptr = add_l2_header(ptr, argv[optind+2], argv[optind]);

  ptr = add_arp_header(ptr, type);
  add_arp_body(ptr, argv[optind], argv[optind+1], argv[optind+2], argv[optind+3]);

  total_length = (l2_header ? ETH_HLEN+ARP_HLEN+ARP_BLEN : ARP_HLEN+ARP_BLEN);

  // Get interface index
  iface_index = if_nametoindex(iface_name);
  if (!iface_index) {
    fprintf(stderr, "Failed to obtain %s interface index: %s\n", iface_name, strerror(errno));
    exit(1);
  }
  
  // Create socket
  socket_fd = l2_header ? socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ARP)) : socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
  if (socket_fd == -1) {
    perror("Failed to open socket");
    exit(1);
  }

  // Get mac address of interface
  strncpy(iface.ifr_name, iface_name, IFNAMSIZ);
  if (ioctl(socket_fd, SIOCGIFHWADDR, &iface) == -1) {
    fprintf(stderr, "Failed to obtain mac address of %s interface: %s", iface.ifr_name, strerror(errno));
    exit(1);
  }

  // Add options
  opts.sll_family = AF_PACKET;
  opts.sll_protocol = htons(ETH_P_ARP);
  opts.sll_ifindex = iface_index; // THIS IS THE ONLY TRULY REQUIRED FIELD
  opts.sll_hatype = ARPHRD_ETHER;
  opts.sll_halen = ETH_ALEN;
  memcpy(opts.sll_addr, broadcast_mac_adress, ETH_ALEN); // FOR ARP REQUESTS, THIS IS THE BROADCAST ADDRESS (ff:ff:ff:ff:ff:ff)

  if (sendto(socket_fd,
  	     buffer,
  	     total_length,
  	     0,
  	     (struct sockaddr *)&opts,
  	     sizeof(opts)) == -1) {
    perror("Failed to send arp request");
    exit(1);
  }

  if (!strncmp(type, "request", 7)) {
    while (1) {
      print_arp_response(socket_fd, total_length);
      sleep(1);
    }
    
  }

  close(socket_fd);
  return 0;
}
