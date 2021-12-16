#include "../headers/mip.h"
#include "../headers/mip_routing.h"
#include "../headers/mip_daemon.h"
#include "../headers/mip_arp.h"
#include "../headers/mip_debug.h"
#include "../headers/utils.h"

#include <string.h>             /* memcpy */
#include <stdio.h>              /* perror */
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>          /* getifaddrs, freeifaddrs */
#include <ifaddrs.h>            /* getifaddrs, freeifaddrs */
#include <sys/types.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <sys/un.h>
#include <sys/socket.h>

int mip_broadcast(const struct network_interfaces *ifs, const uint8_t src, const uint8_t sdu_type, 
    void* sdu, const size_t sdu_len)
{
    int                 wc, i, iovlen = 3;
    struct msghdr       *msg;
    struct iovec        msgvec[iovlen];
    struct mip_pdu      mip_pdu = {0};
    struct frame_header frame_header = {0};
    struct sockaddr_ll  so_name = {0};
    uint8_t broadcast_addr[] = BROADCAST_ADDR;
        
    memcpy(frame_header.src, ifs -> addr[0].sll_addr, MAC_ADDR_LEN);
    memcpy(frame_header.dest, broadcast_addr, MAC_ADDR_LEN);
    frame_header.eth_proto[0] = 0x88;
    frame_header.eth_proto[1] = 0xB5;

    mip_pdu.dest            = MAX_MIP_ADDR;
    mip_pdu.src             = src;
    mip_pdu.ttl             = DEFAULT_TTL;
    mip_pdu.sdu_len         = sdu_len;
    mip_pdu.sdu_type        = sdu_type;

    msgvec[0].iov_base      = &frame_header;
    msgvec[0].iov_len       = sizeof(struct frame_header);
    msgvec[1].iov_base      = &mip_pdu;
    msgvec[1].iov_len       = sizeof(struct mip_pdu);
    msgvec[2].iov_base      = sdu;
    msgvec[2].iov_len       = sdu_len;

    msg = allocate_memory(sizeof(struct msghdr));
    if (msg == NULL)
        return -1;

    msg -> msg_namelen  = sizeof(struct sockaddr_ll);
    msg -> msg_iovlen   = iovlen;
    msg -> msg_iov      = msgvec;

    for (i = 0; i < ifs -> ifs_size; i++)
    {
        memcpy(&so_name, &(ifs -> addr[i]), sizeof(struct sockaddr_ll));    /* our interfaces */
        memcpy(&(so_name.sll_addr), broadcast_addr, MAC_ADDR_LEN);          /* broadcast */
        msg -> msg_name     = &so_name;

        wc = sendmsg(ifs -> raw_socket, msg, 0);
        if (wc <= 0)
        {
            fprintf(stderr, "%s\n", __FUNCTION__);
            perror("sendmsg");
            return wc;
        }
    }

    free(msg);
    return wc;
}

int mip_connect_unix_socket(char *socket_name, char entity)
{
    int sockfd, len, wc;
    struct sockaddr_un sockaddr;
    memset(&sockaddr, 0, sizeof(struct sockaddr_un));

    if ((sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) { /* using SOCK_SEQPACKET to support multiple clients */
        perror("socket");
        return -1;
    }

    memset(&sockaddr, 0, sizeof(struct sockaddr_un));
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, socket_name);
    len = strlen(sockaddr.sun_path) + sizeof(sockaddr.sun_family);

    /* Connecting to socket listener */
    if (connect(sockfd, (struct sockaddr *) &sockaddr, len) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    wc = write(sockfd, &entity, sizeof(char));
    if (wc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int mip_app_recv(int socket, mip_sdu *dest_sdu)
{
    int rc;
    char buf[MAX_MSG_SIZE];

    rc = read(socket, buf, MAX_MSG_SIZE);
	if (rc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("read");
        return -1;
    }

    if (rc == 0)
    {
        // client disconnected, should remove socket from epoll_table
        return -2; // should be some signal code
    }

    dest_sdu -> payload = allocate_memory(rc);
    if (dest_sdu == NULL)
    {
        free(dest_sdu);
        return -1;
    }
    mip_deserialize_sdu(buf, dest_sdu, rc);

    return rc;
}

int mip_app_send(int socket, mip_sdu *sdu)
{
    int wc;
    size_t bufsize = strlen(sdu->payload) + 2;
    char buf[bufsize];

    mip_serialize_sdu(buf, sdu);

    wc = write(socket, buf, bufsize);
    if (wc <= 0)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        return -1;
    }

    return wc;
}

int mip_routing_recv(int socket, char* buf)
{
    int rc;
    char type[4];
    type[3] = '\0';

    rc = read(socket, buf, MAX_RT_PKT_SIZE);
    memcpy(type, buf + 2, 3);

    if (rc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("read");
        return -1;
    }

    if (rc == 0)
    {
        fprintf(stderr, "<daemon>: routing daemon socket closed\n");
        return -2;
    }

    /* else the packet is to be forwarded as a broadcast. Only returning the buffer */
    else if (!strncmp(type, HELLO, 3)) return 1;

    /* else the packet is to be forwarded as a broadcast. Only returning the buffer */
    else if (!strncmp(type, UPDATE, 3)) return 2;

    /* only read packet if it is a lookup packet */
    else if (!strncmp(type, RESPONSEPKT, 3)) return 3;

    fprintf(stderr, "[WARNING]: undefined behaviour in %s at line %d\n", __FUNCTION__, __LINE__);
    return 0;
}

int mip_link_send(arp_entry **arp_table, ifs *ifs, mip_pdu *pdu,
    char *sdu, size_t len, int debug)
{
    int                 wc, iovlen = 3;
    struct arp_entry    arp_entry;
    struct msghdr       *msg;
    struct iovec        msgvec[iovlen];
    struct sockaddr_ll  so_name = {0};
    frame_header        frame_header = {0};
    
    /* if mac address is unknown */
    if (get_arp_entry_by_mip_address(arp_table, &arp_entry, pdu->dest) == 1)
    {
        if (send_arp_request(ifs, pdu->dest, pdu->src) == -1)
            return -1;

        /* we must wait until mac address of destination is known */
        return 1;
    }

    get_arp_entry_by_mip_address(arp_table, &arp_entry, pdu->dest);
    memcpy(frame_header.dest, &arp_entry.dest_mac_addr, MAC_ADDR_LEN);
    memcpy(frame_header.src, &arp_entry.interface, MAC_ADDR_LEN);
    frame_header.eth_proto[0] = 0x88; /* bit shift MIP_P_ETH instead? */
    frame_header.eth_proto[1] = 0xB5; /* bit shift MIP_P_ETH instead? */

    // point to frame header
    msgvec[0].iov_base = &frame_header;
    msgvec[0].iov_len = sizeof(frame_header);

    // point to pdu header
    msgvec[1].iov_base = pdu;
    msgvec[1].iov_len = sizeof(mip_pdu);

    // point to pdu payload
    msgvec[2].iov_base = sdu;
    msgvec[2].iov_len = len;

    msg = allocate_memory(sizeof(struct msghdr));
    if (msg == NULL)
        return -1;

    get_interface_on_ifindex(ifs, &so_name, arp_entry.ifindex);

    msg->msg_name     = &so_name;
    msg->msg_namelen  = sizeof(struct sockaddr_ll);
    msg->msg_iovlen   = iovlen;
    msg->msg_iov      = msgvec;

    wc = sendmsg(ifs -> raw_socket, msg, 0);
    if (wc == -1)
    {
        fprintf(stderr, "%s\n", __FUNCTION__);
        perror("sendmsg");
        free(msg);
        return -1;
    }

    if (debug) 
    {
        mip_debug(arp_table, frame_header.src, frame_header.dest,
            pdu->src, pdu->dest);
    }  

    free(msg);
    return 0;
}

int mip_link_recv(arp_entry **arp_table, ifs *ifs, mip_pdu *pdu, 
    char *buf, uint8_t *arp_addr, int debug)
{
    int                 rc, wc, iovlen = 3;
    struct sockaddr_ll  so_name, sock_if;
    struct msghdr       msg;
    struct iovec        msgvec[iovlen];
    struct frame_header frame_header;
    struct arp_entry    entry_ptr;
    struct mip_arp_sdu  mip_arp_sdu;
    struct mip_sdu      sdu;

    msgvec[0].iov_base  = &frame_header; 
    msgvec[0].iov_len   = sizeof(frame_header);
    msgvec[1].iov_base  = pdu;
    msgvec[1].iov_len   = sizeof(mip_pdu);
    msgvec[2].iov_base  = buf;
    msgvec[2].iov_len   = MAX_MSG_SIZE;

    msg.msg_name        = &so_name;
	msg.msg_namelen     = sizeof(struct sockaddr_ll);
	msg.msg_iovlen      = iovlen;
	msg.msg_iov         = msgvec;
    msg.msg_control     = 0;
    msg.msg_controllen  = 0;
    msg.msg_flags       = 0;

    rc = recvmsg(ifs -> raw_socket, &msg, 0);
    if (rc <= 0)
    {
        perror("recvmsg");
        return -1;
    }
    
    /* prints for both mip and arp communication */
    if (debug) 
    {
        mip_debug(arp_table, frame_header.src, frame_header.dest,
            pdu->src, pdu->dest);
    }  

    if (pdu -> sdu_type == MIP_ARP)
    {
        memcpy((char*) &mip_arp_sdu, buf, sizeof(struct mip_arp_sdu)); /* deserialize arp packet */
        get_interface_on_ifindex(ifs, &sock_if, so_name.sll_ifindex); /* get receiving interface */

        /* if the arp message is a response */
        if (mip_arp_sdu.type == ARP_RES)
        {
            if (debug) 
            {
                printf("<daemon>: got ARP response from %d:\n", pdu->src);
                mip_print_arp_packet(mip_arp_sdu);
            }
            if (add_entry(arp_table, mip_arp_sdu.address, frame_header.src, 
                sock_if.sll_addr, so_name.sll_ifindex) == NULL)
            {
                return -1;
            }

            *arp_addr = mip_arp_sdu.address;
            return 3;
        }

        /* if the arp message is a request */
        else if (mip_arp_sdu.type == ARP_REQ)
        {
            if (debug) 
            {
                printf("<daemon>: got ARP request:\n");
                mip_print_arp_packet(mip_arp_sdu);
            }
            /* add source mip and mac to our arp table */
            /* if we don't find a matching mac address, frame_header.src will not be overwritten */
            if (get_arp_entry_by_mip_address(arp_table, &entry_ptr, pdu -> src))
                if (add_entry(arp_table, pdu -> src, frame_header.src, 
                    sock_if.sll_addr , so_name.sll_ifindex) == NULL)
                {
                    return -1;
                }

            /* replace broadcast address with the source nodes address */
            memcpy(frame_header.dest, frame_header.src, MAC_ADDR_LEN);

            /* if we have stored the requested mip address, store its mac in frame_header.src */
            if (!get_arp_entry_by_mip_address(arp_table, &entry_ptr, mip_arp_sdu.address))
            {
                memcpy(frame_header.src, &entry_ptr.dest_mac_addr, MAC_ADDR_LEN);
                wc = send_arp_response(ifs, arp_table, mip_arp_sdu, frame_header, pdu -> src);
                if (wc == -1) return -1;
            }

            return 4;
        }
    }

    if (pdu -> sdu_type == MIP_ROUTING)
    {
        return 2;
    }

    else if (pdu->sdu_type == MIP_PING)
    {
        if (debug) 
        {
            printf("<daemon>: got ping message from link layer\n");
        }
        mip_deserialize_sdu(buf, &sdu, 2);
        if (sdu.dest == arp_table[0]->mip_address)
        {
            
            if (debug) 
            {
                printf("<daemon>: packet is for us!\n");
            }
            return 0;
        } 
        if (debug) 
        {
            printf("<daemon>: packet is to be forwarded to %d\n", sdu.dest);
        }
        return 1;
    }

    else fprintf(stderr, "<daemon>: %s() undefined behaviour\n", __FUNCTION__);
    return -1;
}

mip_pdu* mip_get_pdu(uint8_t dest, uint8_t src, uint8_t ttl,
    size_t sdu_len, uint8_t sdu_type)
{
    mip_pdu *pdu;

    pdu = allocate_memory(sizeof(mip_pdu));
    if (pdu == NULL)
        return NULL;
        
    pdu -> dest         = dest;
    pdu -> src          = src;
    pdu -> ttl          = ttl;
    pdu -> sdu_len      = sdu_len;
    pdu -> sdu_type     = sdu_type;

    return pdu;
}

int get_mac_from_interface(ifs *ifs)
{
    struct ifaddrs *tmp_ifs;
    int i = 0;

    if (getifaddrs(&tmp_ifs))
    {
        perror("getifaddrs");
        return -1;
    }

    /* storing all outgoing interfaces */
    for (struct ifaddrs *ifptr = tmp_ifs; ifptr != NULL; ifptr = ifptr -> ifa_next) {

		if (ifptr -> ifa_addr != NULL                   && 
            ifptr -> ifa_addr -> sa_family == AF_PACKET && 
            strcmp("lo", ifptr -> ifa_name))
        {
            memcpy(&(ifs -> addr[i++]), (struct sockaddr_ll*) ifptr -> ifa_addr, sizeof(struct sockaddr_ll));
        }
	}

    ifs -> ifs_size = i;
    freeifaddrs(tmp_ifs);
    return 0;
}

int get_interface_on_ifindex(ifs *ifs, struct sockaddr_ll *dest, int ifi)
{
    struct sockaddr_ll ptr;
    int i;
    for (i = 0; i < ifs -> ifs_size; i++)
    {
        ptr = ifs -> addr[i];
        if (ptr.sll_ifindex == ifi)
        {
            memcpy(dest, &ptr, sizeof(struct sockaddr_ll));
            return 0;
        }
    }
    return 1;
}

int mip_send_routing_lookup_request(int socket, uint8_t src, uint8_t dest, int debug)
{
    int wc;

    char buf[REQ_SIZE] = {0};
    buf[0] = src;
    buf[1] = 0;
    buf[2] = 'R';
    buf[3] = 'E';
    buf[4] = 'Q';
    buf[5] = dest;

    if (debug) 
    {
        printf("<daemon>: sending routing lookup request:\n");
    }
    wc = write(socket, buf, REQ_SIZE);
    if (wc == -1)
    {
        fprintf(stderr, "%s() ", __FUNCTION__);
        perror("write");
        return -1;
    }

    return 0;
}

void mip_serialize_sdu(char* dest, mip_sdu *src) 
{
    dest[0] = src->dest;
    dest[1] = src->ttl;
    memcpy(&dest[2], src -> payload, strlen(src -> payload));
}

void mip_deserialize_sdu(char* src, mip_sdu *dest, size_t src_len)
{
    dest->dest      = src[0];
    dest->ttl       = src[1];
    memcpy(dest -> payload, &src[2], src_len - 2);
}
