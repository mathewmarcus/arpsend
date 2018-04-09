#include <netdb.h> /* For gethostbyaddr, gethostbyname, struct hostent */
#include <arpa/inet.h> /* For inet_addr, struct in_addr, inet_ntoa */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define USAGE "%s: [-x] addr_or_hostname\n"

extern int h_errno;

/*
  Simple utility to perform forward (A) or reverse (PTR) DNS lookups

  Purely for academic purposes; in no way a replacement for dig or nslookup
*/
int main(int argc, char *argv[argc])
{
  int c, reverse = 0;
  opterr = 0;
  struct hostent *dns_info = {0};
  char **ptr, *ip_addr_str;
  struct in_addr ip4_addr;

  if (argc < 2) {
    fprintf(stderr, USAGE, argv[0]);
    exit(1);
  }

  while ((c = getopt(argc, argv, "x")) != -1) {
    switch (c) {
    case 'x':
      reverse = 1;
      break;
    case '?':
      fprintf(stderr, USAGE, argv[0]);
      exit(1);
    default:
      break;
    }
  }

  if (optind != argc - 1) {
    fprintf(stderr, USAGE, argv[0]);
    exit(1);
  }

  if (reverse) {
    ip4_addr.s_addr = inet_addr(argv[optind]);
    dns_info = gethostbyaddr(&ip4_addr,
			     4,
			     AF_INET);
    
  }
  else
    dns_info = gethostbyname(argv[optind]);

  if (!dns_info) {
    herror("Failed to find hostname");
    exit(h_errno);
  }
  
  printf("Official name: %s\n", dns_info->h_name);

  ptr = dns_info->h_aliases;
  printf("Aliases: \n");
  while (*ptr) {
    printf("\t%s\n", *ptr++);
  }

  printf("Host address type: %s\n", dns_info->h_addrtype == AF_INET ? "AF_INET" : "AF_INET6");
  printf("Address length: %i\n", dns_info->h_length);

  ptr = dns_info->h_addr_list;
  printf("Addresses: \n");
  while (*ptr) {
    /*
       Here we must do the following
           1. Dereference the char** to char*
           2. Cast the char pointer to an unsigned integer pointer
           3. Dereference the char pointer
           4. Assign the value to the s_addr member of an struct in_addr
           5. Incremement pointer to the next ip address in the h_addr_list member of
              the struct hostent
       
    */
    ip4_addr.s_addr = *((uint32_t *) *ptr++);
    ip_addr_str = inet_ntoa(ip4_addr);

    /*
       Alternatively, since s_addr is the only member of struct in_addr, we can
       cast the char pointer directly to a struct in_addr pointer
    */
    /* ip_addr_str = inet_ntoa(*((struct in_addr *) (*ptr++))); */
    printf("\t%s\n", ip_addr_str);
  }
  return 0;
}
