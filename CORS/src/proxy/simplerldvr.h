// Copyright (C) 2007 Li Tang <tangli99@gmail.com>
//  
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//  
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//  

#ifndef __PROXY_SIMPLERLDVR_H
#define __PROXY_SIMPLERLDVR_H

#include <vector>
#include <string>
#include <sys/time.h>
#include <time.h>
#include <list>
#include <set>
#include <map>
#include <ext/hash_map>
#include "relayadvisor.h"
#include "../common/corsprotocol.h"
#include "eventqueue.h"

using namespace std;
using namespace __gnu_cxx;
using namespace cors;

// net address pair
struct NetPair {
	in_addr_t src_net;
	in_addr_t dst_net;
	NetPair(in_addr_t src = 0, in_addr_t dst = 0)
		: src_net(src), dst_net(dst) {}
};

// set of relay network addr
struct Record {
	time_t update;
	NetSet filter;
};

struct HashFunc {
	size_t operator()(const NetPair &key) const {
		size_t __h = 0;
		uint8_t buf[2*sizeof(in_addr_t)];
		memcpy(buf, &key.src_net, sizeof(in_addr_t));
		memcpy(buf + sizeof(in_addr_t), &key.dst_net, sizeof(in_addr_t));
		for (int i = 0; i < 2*sizeof(in_addr_t); i++) {
			__h = 5 * __h + buf[i];
		}
		return __h;
	}
};

struct CompFunc {
	bool operator()(const NetPair &k1, const NetPair &k2) const {
		return ((k1.src_net == k2.src_net) && 
				(k1.dst_net == k2.dst_net));
	}
};

typedef hash_map<const NetPair, Record, HashFunc, CompFunc> NetHMap;
//typedef set<in_addr_t> NetSet;

class SimpleRldvr : public RelayAdvisor {
public:
  enum PARAM_INDEX { 
		DB_ADDR, 
		DB_PORT
	};

  SimpleRldvr(vector<string> &param);
	~SimpleRldvr() {}
	bool initialize(vector<string> &param);

  virtual void start(int sk, NeighborMaintainer *nbmtr) {}
  virtual void update(int sk, NeighborMaintainer *nbmtr) {}
  virtual time_t get_update_interval() { return 0; }
  virtual void process_rl_inquiry(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr);
  virtual void process_rl_report(char * msg, int len, sockaddr_in & peeraddr, int sk);
  virtual void process_rl_filter(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr);

private:
//	struct NetPair {
//		in_addr_t src_net;
//		in_addr_t dst_net;
//		bool operator<
//	};

//	struct NetUnit {
//		in_addr_t net_addr;
//		time_t update;
//		NetUnit(in_addr_t net = 0, time_t upd = 0) 
//			: net_addr(net), update(upd) {}
//	};

//	typedef set<NetUnit> NetSet;
//	typedef map<NetPair, set<NetUnit> > NetMap;
#if 0
	struct Knowledge {
		uint16_t rly_id;	// AS id
		uint16_t reserved;	// padding
		uint32_t delay;
		Knowledge() {
			memset(this, 0, sizeof(Knowledge));
		}
		Knowledge(uint16_t rid, uint
		}
	};

		}
	};


32_t dly) : rly_id(rid), delay(dly) {
			reserved = 0;
		}
	};


	typedef list<Knowledge> KnList;
#endif


	char *pack_rl_inquiry( 	int 		&len, 
						 	uint8_t 	rl_num,
							in_port_t	src_port,
							in_addr_t	src_ip,
							in_addr_t 	src_mask,
						 	in_addr_t 	dst_ip, 
							in_addr_t	dst_mask,
						 	time_t		req_update, 
						 	uint32_t 	req_delay);

	int parse_rl_inquiry(	const char 	*msg, 
						 	int 		len, 
						 	uint8_t 	&rl_num,
							in_addr_t	&src_ip,
							in_addr_t	&src_mask,
							in_addr_t	&dst_ip,
						 	in_addr_t 	&dst_mask, 
						 	time_t		&req_update, 
						 	uint32_t 	&req_delay);

	char *pack_rl_advice(	const char 	*inquiry_msg,
							int 		&len, 
					   	 	uint8_t 	rl_num, 
					   	 	const RluList &rl_list);
	
	int parse_rl_advice( 	const char 	*msg, 
						 	int 		len, 
							uint8_t 	&rl_num, 
							RluList		&rl_list);
	
	char *pack_rl_filter(	const char 	*inquiry_msg,
							int 		&len,
							uint8_t	net_num,
							const NetSet &net_set);

	int parse_rl_filter(	const char	*msg,
							int			len,
							uint8_t	&net_num,
							in_port_t	&src_port,
							in_addr_t	&src_ip,
							in_addr_t 	&src_mask,
						 	in_addr_t 	&dst_ip, 
							in_addr_t	&dst_mask,
							NetSet		&net_set);

#if 0	
	char *pack_rl_share(	int			&len,
							in_addr_t	src_ip,
							in_addr_t	dst_ip,
							in_addr_t	rly_ip,
							timeval		update,
							uint32_t	delay);
	
	int parse_rl_share(		const char 	*msg,
							int			len,
							in_addr_t	&src_ip,
							in_addr_t	&dst_ip,
							in_addr_t	&rly_ip,
							timeval		&update,
							uint32_t	&delay);

	char *pack_rl_report(	int			&len,
							uint16_t	src_id,		// AS id
							uint16_t	dst_id,
							uint16_t	kn_num,
							const KnList &kn_list);	
	
	int parse_rl_report(	const char	*msg,
							int			len,
							uint16_t	&src_id,
							uint16_t	&dst_id,
							uint16_t	&kn_num,
							KnList		&kn_list);
#endif

	in_addr_t db_addr;
	in_port_t db_port;

	NetHMap local_cache;

//    std::string conf_file;
	char buff[cors::CORS_MAX_MSG_SIZE];
};

#endif
