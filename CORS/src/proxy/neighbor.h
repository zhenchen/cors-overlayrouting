#ifndef __PROXY_NEIGHBOR_H
#define __PROXY_NEIGHBOR_H

#include <netinet/in.h>

struct Neighbor {
	enum NB_TYPE { ORACLE, PEER };
	Neighbor(char ty = PEER, in_addr_t a = 0,
			 in_port_t p = 0, time_t t = 0,
			 in_addr_t m = 0xFFFFFFFF, uint16_t as = 0,
			 double r = 0, uint8_t s = 0)
		: type(ty), addr(a), port(p), ts(t),
		  mask(m), asn(as), rtt(r), seq(s) {}
	char type;
	in_addr_t addr;
	in_port_t port;
	time_t ts;
	in_addr_t mask;
	uint16_t asn;
	double rtt;
	uint8_t seq;
};

#endif
