#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define UDP_PORT_NUM 16
// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

struct ports_packets{
  int port;
  int front;
  int tail;
  struct proc *p;
  char* que[16];
};

static struct ports_packets *udp_ports_packets[UDP_PORT_NUM];
static int port_count = 0;

void
netinit(void)
{
  initlock(&netlock, "netlock");
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //
  acquire(&netlock);
  if(port_count > 15){
    printf("sys_bind: udp port full\n");
    release(&netlock);
    return -1;
  }

  struct ports_packets *newpp = kalloc();
  int port;

  if(newpp == 0){
    printf("sys_bind: kalloc failed\n");
    release(&netlock);
    return -1;
  }

  argint(0, &port);

  newpp->port = port;
  newpp->front = 0;
  newpp->tail = 0;

  udp_ports_packets[port_count] = newpp;
  port_count++;
  release(&netlock);
  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //
  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  //
  // Your code here.
  //
  struct proc *p = myproc();
  int dport;
  uint64 srcaddr;
  uint64 sportaddr;
  uint64 bufaddr;
  int maxlen;
  struct ports_packets *ps_ps = 0;
  int front;
  char* packet;

  argint(0, &dport);
  argaddr(1, &srcaddr);
  argaddr(2, &sportaddr);
  argaddr(3, &bufaddr);
  argint(4, &maxlen);

  int total = maxlen + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  acquire(&netlock);
  for(int i = 0; i < port_count; i++){
    if(udp_ports_packets[i]){
      if(udp_ports_packets[i]->port == dport){
        ps_ps = udp_ports_packets[i];
        break;
      }
    }
  }

  if(ps_ps == 0){
    return -1;
  }

  front = ps_ps->front;
  while(ps_ps->front == ps_ps->tail){
    sleep(ps_ps, &netlock);
  }

  packet = ps_ps->que[front];
  struct eth *eth_hdr = (struct eth *)packet;
  struct ip *ip_hdr = (struct ip *)(eth_hdr + 1);
  struct udp *udp_hdr = (struct udp *)(ip_hdr + 1);
  char* load = (char*)(udp_hdr + 1);

  uint32 src = ntohl(ip_hdr->ip_src);
  printf("sys_recv: the src ip is %x\n", ip_hdr->ip_src);
  copyout(p->pagetable, srcaddr, (void*)&src, 4);

  uint16 sport = ntohs(udp_hdr->sport);
  copyout(p->pagetable, sportaddr, (void*)&sport, 2);

  int flag = copyout(p->pagetable, bufaddr, load, maxlen);
  int data_len = ntohs(udp_hdr->ulen) - 8;

  ps_ps->front = (front + 1) % 16;
  kfree((void*)ps_ps->que[front]);

  release(&netlock);

  if(flag == 0){
    return data_len;
  }

  return -1;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //
  struct eth *eth_hdr = (struct eth *)buf;
  struct ip *ip_hdr = (struct ip *)(eth_hdr + 1);
  struct udp *udp_hdr = (struct udp *)(ip_hdr + 1);
  struct ports_packets *ps_ps;

  uint16 dport = ntohs(udp_hdr->dport);

  if(ip_hdr->ip_p != IPPROTO_UDP){
    //drop
    kfree((void *)buf);
    return;
  }

  acquire(&netlock);
  int binded = 0;
  for(int i = 0; i < port_count; i++){
    if(udp_ports_packets[i]){
      if(udp_ports_packets[i]->port == dport){
        ps_ps = udp_ports_packets[i];
        binded = 1;
        break;
      }
    }
  }

  if(binded){
    int front = ps_ps->front;
    int tail = ps_ps->tail;
    if((tail + 1) % 16 == front){
      //drop
      kfree((void*) buf);

      release(&netlock);
      return;
    }

    ps_ps->que[tail] = buf;
    ps_ps->tail = (tail + 1) % 16;
    printf("ip_rx: put packet on the que\n");
    printf("ip_rx: the src of the packet is %x\n", ntohl(ip_hdr->ip_src));
    wakeup(ps_ps);
  }else{
    //drop
    printf("ip_rx: %d haven't been binded , drop\n", dport);
    kfree((void*)buf);
  }

  release(&netlock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
