// Copyright (C) 2007 Hui Zhang <zhang.w.hui@gmail.com>
// //  
// // This program is free software; you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation; either version 2 of the License, or
// // (at your option) any later version.
// //  
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //  
// // You should have received a copy of the GNU General Public License
// // along with this program; if not, write to the Free Software
// // Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//  


#ifndef __PROXY_SIMPLEFWD_H
#define __PROXY_SIMPLEFWD_H

#include <ext/hash_map>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "packetforwarder.h"
#include "../common/corsprotocol.h"
#include "../utils/debug.h"

#pragma pack(1)
using namespace std;
using namespace __gnu_cxx;
using namespace cors;

const uint8_t DATA_ADDR_OFFSET = 3;
const uint8_t MIN_DATA_PKT_LEN = 9;
const uint32_t MAX_FWD_BW = 1024*1024*2;

//hash table definition
struct session_key {       
	uint16_t src_port;
	uint32_t src_addr;
	uint16_t dst_port;
	uint32_t dst_addr; 
};
const uint8_t KEY_LEN = sizeof(session_key);

struct fwd_records {
	fwd_records(timeval t, uint16_t bw=0, uint32_t p=0, uint64_t b=0)
		:start_time(t), end_time(t), last_contact(t), rq_bw(bw), fwd_packet(p), fwd_byte(b) {}
	struct timeval start_time;
	struct timeval end_time;
	struct timeval last_contact;
	uint16_t rq_bw;								        
	uint32_t fwd_packet;
	uint64_t fwd_byte;
};

struct hash_byte {
	size_t operator()(const session_key k) const {
		uint16_t __h = 0;
		uint8_t * g = (uint8_t*)(&k);
		for(size_t i = 0; i < KEY_LEN; i++)	{
			__h = 5 * __h + (*(g+i));
		}
		return size_t(__h);
	}	
};

struct cmp_byte {
	bool operator()(const session_key k1, const session_key k2) const {	
		return memcmp(&k1, &k2, KEY_LEN)==0;
	}
};
typedef hash_map<const session_key, fwd_records, const hash_byte, cmp_byte> Fwd_HMap;


// Release the resource of the timeout flow
void* update_daemon(void *);


class SimpleFwd : public PacketForwarder {
	friend void* update_daemon(void *);

public:
	enum PARAM_INDEX { TIMEOUT_SEC, UPDATE_INTERVAL, MAX_FLOW, LOG_FLAG, LOG_FILE };
	SimpleFwd (vector<string> & param);
	~SimpleFwd ();

	virtual void process_probe(char * msg, int len, sockaddr_in & peeraddr, int sk);
	virtual void process_bw_reserve(char * msg, int len, sockaddr_in & peeraddr, int sk);
	virtual void process_bw_release(char * msg, int len, sockaddr_in & peeraddr, int sk);
	virtual void process_data_packet(char * msg, int len, sockaddr_in & peeraddr, int sk);

private:
	time_t _timeout_sec;
	time_t _update_interval;
	uint8_t _max_flow;
	uint16_t _cur_rq_bw;
	
	bool _log_flag;
	string _log_file;
//  Control strct_ctrl;
	
	pthread_t _update_id;
	pthread_mutex_t _fwd_hmap_mutex;
	Fwd_HMap _fwd_hmap;
	ofstream _ofs;

	void parse_param(vector<string> &param);
	void update_fwd_hmap();
	void log_fwd_record(ostream &os, Fwd_HMap::iterator iter);
	void log_load(ostream &os, struct timeval tv);
};

#endif


