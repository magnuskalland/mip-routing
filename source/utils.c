#include "../headers/utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <stdlib.h>

void *allocate_memory(size_t size)
{
    void *ptr;
    ptr = calloc(1, size);
    if (ptr == NULL)
    {
        fprintf(stderr, "calloc (in allocate_memory): \n");
    }
    return ptr;
}

unsigned int get_8bit_number()
{
    time_t t;
    srand((unsigned) time(&t));
    return rand() % 255; 
}

int in_range(int a, int x, int y)
{
    return (a >= x && a <= y);
}

void print_mac_address(uint8_t addr[], size_t len)
{
    size_t i;
	for (i = 0; i < len - 1; i++) {
		printf("%d:", addr[i]);
	}
	printf("%d\n", addr[i]);
}

void print_sockaddr_ll(struct sockaddr_ll *sockaddr)
{
    printf("\n%35s\n", " --- SOCKADDR_LL START ---");
    printf("%30s: %u\n", "sll_family", sockaddr -> sll_family);
    printf("%30s: %u\n", "sll_protocol", sockaddr -> sll_protocol);
    printf("%30s: %d\n", "sll_ifindex", sockaddr -> sll_ifindex);
    printf("%30s: %u\n", "sll_hatype", sockaddr -> sll_hatype);
    printf("%30s: %u\n", "sll_pkttype", sockaddr -> sll_pkttype);
    printf("%30s: %u\n", "sll_halen", sockaddr -> sll_halen);
    printf("%30s: ", "sll_addr"); print_mac_address(sockaddr -> sll_addr, 6);
    printf("%35s\n\n", " --- SOCKADDR_LL END ---");
}

void print_string_as_binary(char* string)
{
    char *s = string;
    int i;
    unsigned char c;

    while (*s)
    {
        c = s[0];
        for (i = 7; i >= 0; i-- ) 
        {
            printf( "%d", ( c >> i ) & 1 ? 1 : 0 );
            if (i == 4)
                printf(" ");
        }
        printf("%s", " | ");
        s++;
    }
    
    printf("\n");
}

double diff_time_ms(struct timespec start, struct timespec end)
{
	double s, ms, ns;

	s  = (double) end.tv_sec  - (double) start.tv_sec;
	ns = (double) end.tv_nsec - (double) start.tv_nsec;

	if (ns < 0) { // clock underflow
		--s;
		ns += 1000000000;
	}

	ms = ((s) * 1000 + ns/1000000.0);

	return ms;
}