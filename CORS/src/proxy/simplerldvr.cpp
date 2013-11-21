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

#include <iostream>
#include <sstream>
#include <fstream>
//#include <stdexcept>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "simplerldvr.h"
#include "simplenbmtr.h"
#include "neighbor.h"
#include "../utils/debug.h"
#include "../utils/utilfunc.h"

using namespace std;
using namespace cors;


SimpleRldvr::SimpleRldvr(vector<string> & param)
	: db_addr(0), db_port(0)
{
	initialize(param);
}

bool
SimpleRldvr::initialize(vector<string> &param) 
{
	vector<string>::size_type n = param.size();
	if (n <= DB_ADDR) return false;
	istringstream iss1(param[DB_ADDR]);
	string db_addr_str;
	iss1 >> db_addr_str;
	db_addr = inet_addr(db_addr_str.c_str());

	if (n <= DB_PORT) return false;
	istringstream iss2(param[DB_PORT]);
	iss2 >> db_port;
	db_port = htons(db_port);

	in_addr temp;
	temp.s_addr = db_addr;
	DEBUG_PRINT(3, "db_addr = %s\n", inet_ntoa(temp));
	DEBUG_PRINT(3, "db_port = %d\n", ntohs(db_port));

	return true;
}

void
SimpleRldvr::process_rl_inquiry(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr)
{
	//SimpleNbmtr *snbmtr = dynamic_cast<SimpleNbmtr *>(nbmtr);
	assert(nbmtr != NULL);

#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
	time_t t = time(NULL);
	char *ct = ctime(&t);
	ct[strlen(ct) - 1] = '\0';
	cout << "[" << ct << "]:" << "SimpleRldvr::process_rl_inquiry ("
		 << sockaddr2str(peeraddr) << ")" << endl;
	cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
#endif
	
	int neighbor_num = nbmtr->get_neighbor_num();
	if (neighbor_num == 0) return;

	uint8_t rl_num = 0;
	in_addr_t src_ip = 0;
	in_addr_t src_mask = 0;
	in_addr_t dst_ip = 0;
	in_addr_t dst_mask = 0;
	time_t req_update = 0;
	uint32_t req_delay = 0;

	int ret = parse_rl_inquiry(msg, len, rl_num, 
							   src_ip, src_mask, 
							   dst_ip, dst_mask, 
							   req_update, req_delay);
	if (ret < 0) {
		DEBUG_PRINT(2, "Parsing inquiry error\n");
		return;
	}

	msg[Inquiry::TTL_POS]--;
	DEBUG_PRINT(3, "neighbors = %d\n", nbmtr->get_neighbor_num());

	if (nbmtr->get_neighbor_num() < rl_num) {
		if (msg[Inquiry::TTL_POS] > 0) {
			// forward to another proxy
			RelayUnit rl_unit;
			nbmtr->get_neighbor(0, rl_unit);

			msg[Inquiry::NUM_POS] -= nbmtr->get_neighbor_num();

			sockaddr_in proxyaddr;
			memset(&proxyaddr, 0, sizeof(sockaddr_in));
			proxyaddr.sin_family = AF_INET;
			proxyaddr.sin_port = rl_unit.port;
			proxyaddr.sin_addr.s_addr = rl_unit.addr;
	
			sendto(sk, msg, len, 0, (sockaddr *)&proxyaddr, sizeof(sockaddr));
#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
			time_t t = time(NULL);
			char *ct = ctime(&t);
			ct[strlen(ct) - 1] = '\0';
			cout << "[" << ct << "]:" << "SimpleRldvr::send inquiry to proxy ("
				 << sockaddr2str(proxyaddr) << ")" << endl;
			cout << "-->: " << bytes2str((unsigned char *)msg, len) << endl;
#endif
			msg[Inquiry::NUM_POS] = rl_num;
		}
	}
#if 0
	NetPair net_pair(src_ip & src_mask, dst_ip & dst_mask);
	NetHMap::iterator iter = local_cache.find(net_pair);
	if (iter == local_cache.end()) {
		sockaddr_in database;
		memset(&database, 0, sizeof(sockaddr_in));
		database.sin_family = AF_INET;
		database.sin_port = db_port;
		database.sin_addr.s_addr = db_addr;

		sendto(sk, msg, len, 0, (sockaddr *)&database, sizeof(sockaddr));
#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
		time_t t = time(NULL);
		char *ct = ctime(&t);
		ct[strlen(ct) - 1] = '\0';
		cout << "[" << ct << "]:" << "SimpleRldvr::send inquiry to database ("
			 << sockaddr2str(database) << ")" << endl;
		cout << "-->: " << bytes2str((unsigned char *)msg, len) << endl;
#endif
		return;
	} else {
#endif
	{
		NList nb_list;
		nbmtr->fetch_neighbor_list(nb_list);	
	
		RluList rl_list;
		NList::reverse_iterator rit = nb_list.rbegin();
		for (; rit != nb_list.rend(); ++rit) {
//			if (tm_now - (*iter)->ts <= update) {
				rl_list.push_back(RelayUnit((*rit)->addr,
											(*rit)->mask,
											(*rit)->port,
											(*rit)->ts, 0));
//			}
		}
//			rl_num = nbmtr->fetch_candidate(rl_num, req_update, iter->second.filter, rl_list);
		rl_num = rl_list.size();
		int adv_len = 0;
		char *adv_msg = pack_rl_advice(msg, adv_len, rl_num, rl_list);
		
		sockaddr_in client;
		memset(&client, 0, sizeof(sockaddr_in));
		client.sin_family = AF_INET;
		memcpy(&client.sin_port, msg + Inquiry::SRC_PORT_POS, 6);
		
		sendto(sk, adv_msg, adv_len, 0, (sockaddr *)&client, sizeof(sockaddr));
#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
		time_t t = time(NULL);
		char *ct = ctime(&t);
		ct[strlen(ct) - 1] = '\0';
		cout << "[" << ct << "]:" << "SimpleRldvr::send advice to client ("
			 << sockaddr2str(client) << ")" << endl;
		cout << "-->: " << bytes2str((unsigned char *)adv_msg, adv_len) << endl;
#endif
		delete[] adv_msg;
		
	//	time_t tm_now = time(NULL);
	//	if (tm_now - iter->second.update > 300) {
	//		local_cache.erase(net_pair);
	//	}
		return;
	}

	//SimpleNbmtr::NList::reverse_iterator iter = snbmtr->nl.rbegin();
	//int i = 0;
	//for ( ; i < rl_num && iter != snbmtr->nl.rend(); i++, ++iter) {
	//	if (tm_now - (*iter)->ts <= req_update) {
	//		rl_list.push_back(RelayUnit((*iter)->addr,
	//									(*iter)->mask,
	//									(*iter)->port,
	//									(*iter)->ts,
	//									0));
	//	}
	//}
	//if (i < rl_num) rl_num = i+1;

}

void
SimpleRldvr::process_rl_report(char * msg, int len, sockaddr_in & peeraddr, int sk)
{
	// Simply redirect the report packet to database server
	sockaddr_in database;
	memset(&database, 0, sizeof(sockaddr_in));
	database.sin_family = AF_INET;
	database.sin_port = db_port;
	database.sin_addr.s_addr = db_addr;

	sendto(sk, msg, len, 0, (sockaddr *)&database, sizeof(sockaddr));
#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
	time_t t = time(NULL);
	char *ct = ctime(&t);
	ct[strlen(ct) - 1] = '\0';
	cout << "[" << ct << "]:" << "SimpleRldvr::process_rl_report ("
		 << sockaddr2str(peeraddr) << ")" << endl;
	cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
	cout << "[" << ct << "]:" << "SimpleRldvr::send report to "
		 << sockaddr2str(database) << endl;
#endif
}

void
SimpleRldvr::process_rl_filter(char * msg, int len, sockaddr_in & peeraddr, int sk, NeighborMaintainer *nbmtr) 
{
	Record record;

	uint8_t net_num;
	in_port_t src_port;
	in_addr_t src_ip, src_mask;
	in_addr_t dst_ip, dst_mask;

	int ret = parse_rl_filter(msg, len, 
							  net_num, src_port, 
							  src_ip, src_mask,
							  dst_ip, dst_mask,
							  record.filter);

	time_t tm_now = time(NULL);
	NetPair net_pair(src_ip & src_mask, dst_ip & dst_mask);
	record.update = tm_now;

	local_cache.insert(pair<const NetPair, Record>(net_pair, record));

	DEBUG_PRINT(3, "neighbors = %d\n", nbmtr->get_neighbor_num());
	
	NList nb_list;
	nbmtr->fetch_neighbor_list(nb_list);	
	
	RluList rl_list;
	NList::reverse_iterator iter = nb_list.rbegin();
	for (; iter != nb_list.rend(); ++iter) {
//		if (tm_now - (*iter)->ts <= update) {
		rl_list.push_back(RelayUnit((*iter)->addr,
									(*iter)->mask,
									(*iter)->port,
									(*iter)->ts, 0));
//		}
	}
	int rl_num = rl_list.size();
//	int rl_num = nbmtr->fetch_candidate(MAX_RL_NUM, 500, record.filter, rl_list);
	int adv_len = 0;
	char *adv_msg = pack_rl_advice(msg, adv_len, rl_num, rl_list);

	sockaddr_in client;
	memset(&client, 0, sizeof(sockaddr_in));
	client.sin_family = AF_INET;
	client.sin_port = src_port;
	client.sin_addr.s_addr = src_ip;

	sendto(sk, adv_msg, adv_len, 0, (sockaddr *)&client, sizeof(sockaddr));
#if (defined(RLDVR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
	time_t t = time(NULL);
	char *ct = ctime(&t);
	ct[strlen(ct) - 1] = '\0';
	cout << "[" << ct << "]:" << "SimpleRldvr::process_rl_filter ("
		 << sockaddr2str(peeraddr) << ")" << endl;
	cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
	cout << "[" << ct << "]:" << "SimpleRldvr::send advice to client ("
		 << sockaddr2str(client) << ")" << endl;
	cout << "-->: " << bytes2str((unsigned char *)adv_msg, adv_len) << endl;
#endif
	delete[] adv_msg;
			
}

char *
SimpleRldvr::pack_rl_inquiry(int &len, uint8_t rl_num, in_port_t src_port,
							 in_addr_t src_ip, in_addr_t src_mask,
							 in_addr_t dst_ip, in_addr_t dst_mask,
							 /*in_port_t prx_port, in_addr_t prx_ip,*/
							 time_t req_update, uint32_t req_delay)
{
	len = Inquiry::INQUIRY_LEN;
	char *msg = new char[len];
	
	memset(msg, 0, len);

	PACKET_TYPE type = TYPE_RLY_INQUIRY;
	memcpy(msg + Inquiry::TYPE_POS, &type, Inquiry::TYPE_LEN);
	
	memset(msg + Inquiry::TTL_POS, MAX_TTL, Inquiry::TTL_LEN);

	memcpy(msg + Inquiry::NUM_POS, &rl_num, Inquiry::NUM_LEN);
	
	// src/dst_ip should be in network order
	memcpy(msg + Inquiry::SRC_PORT_POS, &src_port, Inquiry::SRC_PORT_LEN);
	memcpy(msg + Inquiry::SRC_ADDR_POS, &src_ip, Inquiry::SRC_ADDR_LEN);
	memcpy(msg + Inquiry::SRC_MASK_POS, &src_mask, Inquiry::SRC_MASK_LEN);

	memcpy(msg + Inquiry::DST_ADDR_POS, &dst_ip, Inquiry::DST_ADDR_LEN);
	memcpy(msg + Inquiry::DST_MASK_POS, &dst_mask, Inquiry::DST_MASK_LEN);

//	memcpy(msg + Inquiry::PRX_PORT_POS, &prx_port, Inquiry::PRX_PORT_LEN);
//	memcpy(msg + Inquiry::PRX_ADDR_POS, &prx_ip, Inquiry::PRX_ADDR_LEN);

//	timeval tm_now;
//	gettimeofday(&tm_now, NULL);
//	memcpy(msg + QA_TS_POS, &tm_now, QA_TS_LEN);
	memcpy(msg + Inquiry::UPD_POS, &req_update, Inquiry::UPD_LEN);
	memcpy(msg + Inquiry::DLY_POS, &req_delay, Inquiry::DLY_LEN);
	
	return msg;
}

int 
SimpleRldvr::parse_rl_inquiry(const char *msg, int len, uint8_t &rl_num,
							  in_addr_t &src_ip, in_addr_t &src_mask,
					 		  in_addr_t &dst_ip, in_addr_t &dst_mask,
							  time_t &req_update, uint32_t &req_delay)
{
	assert(len >= 1 && msg[Inquiry::TYPE_POS] == TYPE_RLY_INQUIRY);
	len -= Inquiry::TYPE_LEN;
	len -= Inquiry::TTL_LEN;

	if (len < Inquiry::NUM_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: rl_num\n");
		return -1;
	}
	memcpy(&rl_num, msg + Inquiry::NUM_POS, Inquiry::NUM_LEN);
	len -= Inquiry::NUM_LEN;

	len -= Inquiry::SRC_PORT_LEN;

	if (len < Inquiry::SRC_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: src_ip\n");
		return -1;
	}
	memcpy(&src_ip, msg + Inquiry::SRC_ADDR_POS, Inquiry::SRC_ADDR_LEN);
	len -= Inquiry::SRC_ADDR_LEN;

	if (len < Inquiry::SRC_MASK_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: src_mask\n");
		return -1;
	}
	memcpy(&src_mask, msg + Inquiry::SRC_MASK_POS, Inquiry::SRC_MASK_LEN);
	len -= Inquiry::SRC_MASK_LEN;

	if (len < Inquiry::DST_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: dst_ip\n");
		return -1;
	}
	memcpy(&dst_ip, msg + Inquiry::DST_ADDR_POS, Inquiry::DST_ADDR_LEN);
	len -= Inquiry::DST_ADDR_LEN;

	if (len < Inquiry::DST_MASK_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: dst_mask\n");
		return -1;
	}
	memcpy(&dst_mask, msg + Inquiry::DST_MASK_POS, Inquiry::DST_MASK_LEN);
	len -= Inquiry::DST_MASK_LEN;

	len -= (Inquiry::PRX_PORT_LEN + Inquiry::PRX_ADDR_LEN);

	if (len < Inquiry::UPD_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: req_update\n");
		return -1;
	}
	memcpy(&req_update, msg + Inquiry::UPD_POS, Inquiry::UPD_LEN);
	len -= Inquiry::UPD_LEN;

	if (len < Inquiry::DLY_LEN) {
		DEBUG_PRINT(2, "Incomplete inquiry message: req_delay\n");
		return -1;
	}
	memcpy(&req_delay, msg + Inquiry::DLY_POS, Inquiry::DLY_LEN);

	return 0;
}

char *
SimpleRldvr::pack_rl_advice(const char *inquiry_msg, 
							int &len, uint8_t rl_num,
					 		const RluList &rl_list)
{
	if (rl_num > MAX_RL_NUM) {
//		DEBUG_PRINT(2, "Cannot send more than %d relay units in a advice packet\n", MAX_RL_NUM);
		rl_num = MAX_RL_NUM;
	}
	if (rl_num > rl_list.size()) {
		rl_num = rl_list.size();
	}

	len = Advice::RLS_POS + rl_num * sizeof(RelayUnit); //16 + rl_num * 24
	char *msg = new char[len];

	memset(msg, 0, len);

	PACKET_TYPE type = TYPE_RLY_ADVICE;
	memcpy(msg + Advice::TYPE_POS, &type, Advice::TYPE_LEN);
	memcpy(msg + Advice::NUM_POS, &rl_num, Advice::NUM_LEN);

	// copy from inquiry msg
	memcpy(msg + Advice::SRC_PORT_POS, 
		   inquiry_msg + Inquiry::SRC_PORT_POS,
		   Advice::RLS_POS - Advice::SRC_PORT_POS);

//	timeval tm_now;
//	gettimeofday(&tm_now, NULL);
//	memcpy(msg + QA_TS_POS, &tm_now, QA_TS_LEN);

	RluList::const_iterator iter;
	int i = 0, pos = Advice::RLS_POS;
	for (i = 0, iter = rl_list.begin(); i < rl_num; i++, ++iter) {
		memcpy(msg + pos, &(*iter), sizeof(RelayUnit));
		pos += sizeof(RelayUnit);
	}

	return msg;
}

int 
SimpleRldvr::parse_rl_advice(const char *msg, int len, 
							 uint8_t &rl_num, RluList &rl_list)
{
	assert(len >= 1 && msg[Advice::TYPE_POS] == TYPE_RLY_ADVICE);
	len -= Advice::TYPE_LEN;

	if (len < Advice::NUM_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: rl_num\n");
		return -1;
	}
	memcpy(&rl_num, msg + Advice::NUM_POS, Advice::NUM_LEN);
	len -= Advice::NUM_LEN;

	len -= (Advice::RLS_POS - Advice::SRC_PORT_POS);

	RelayUnit rl_unit;
	int pos = Advice::RLS_POS;
	for (int i = 0; i < rl_num; i++) {
		if (len < sizeof(RelayUnit)) {
			DEBUG_PRINT(2, "Incomplete advice message: rl_unit[%d]\n", i);
			return -1;
		}
		memcpy(&rl_unit, msg + pos, sizeof(RelayUnit));
		pos += sizeof(RelayUnit);
		len -= sizeof(RelayUnit);
		rl_list.push_back(rl_unit);
	}

	return 0;
}

char *
SimpleRldvr::pack_rl_filter(const char *inquiry_msg,
							int &len, uint8_t net_num,
							const NetSet &net_set)
{
	if (net_num > MAX_FILTER_SIZE) {
		net_num = MAX_FILTER_SIZE;
	}
	if (net_num > net_set.size()) {
		net_num = net_set.size();
	}

	len = Filter::RPRT_POS + net_num * sizeof(in_addr_t);
	char *msg = new char[len];

	memset(msg, 0, len);

	PACKET_TYPE type = TYPE_RLY_FILTER;
	memcpy(msg + Filter::TYPE_POS, &type, Filter::TYPE_LEN);
	memcpy(msg + Filter::NUM_POS, &net_num, Filter::NUM_LEN);

	// copy from inquiry msg
	memcpy(msg + Filter::SRC_PORT_POS, 
		   inquiry_msg + Inquiry::SRC_PORT_POS,
		   Filter::RPRT_POS - Filter::SRC_PORT_POS);

//	timeval tm_now;
//	gettimeofday(&tm_now, NULL);
//	memcpy(msg + QA_TS_POS, &tm_now, QA_TS_LEN);

	NetSet::const_iterator iter;
	int i = 0, pos = Filter::RPRT_POS;
	for (i = 0, iter = net_set.begin(); i < net_num; i++, ++iter) {
		memcpy(msg + pos, &(*iter), sizeof(in_addr_t));
		pos += sizeof(in_addr_t);
	}

	return msg;
}

int 
SimpleRldvr::parse_rl_filter(const char *msg, int len, 
							 uint8_t &net_num, in_port_t &src_port,
							 in_addr_t &src_ip, in_addr_t &src_mask,
							 in_addr_t &dst_ip, in_addr_t &dst_mask,
							 NetSet &net_set)
{
	assert(len >= 1 && msg[Filter::TYPE_POS] == TYPE_RLY_FILTER);
	len -= Filter::TYPE_LEN;
	len -= Filter::TTL_LEN;

	if (len < Filter::NUM_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: net_num\n");
		return -1;
	}
	memcpy(&net_num, msg + Filter::NUM_POS, Filter::NUM_LEN);
	len -= Filter::NUM_LEN;

	if (len < Filter::SRC_PORT_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: src_port\n");
		return -1;
	}
	memcpy(&src_port, msg + Filter::SRC_PORT_POS, Filter::SRC_PORT_LEN);
	len -= Filter::SRC_PORT_LEN;

	if (len < Filter::SRC_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: src_ip\n");
		return -1;
	}
	memcpy(&src_ip, msg + Filter::SRC_ADDR_POS, Filter::SRC_ADDR_LEN);
	len -= Filter::SRC_ADDR_LEN;

	if (len < Filter::SRC_MASK_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: src_mask\n");
		return -1;
	}
	memcpy(&src_mask, msg + Filter::SRC_MASK_POS, Filter::SRC_MASK_LEN);
	len -= Filter::SRC_MASK_LEN;

	if (len < Filter::DST_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: dst_ip\n");
		return -1;
	}
	memcpy(&dst_ip, msg + Filter::DST_ADDR_POS, Filter::DST_ADDR_LEN);
	len -= Filter::DST_ADDR_LEN;

	if (len < Filter::DST_MASK_LEN) {
		DEBUG_PRINT(2, "Incomplete advice message: dst_mask\n");
		return -1;
	}
	memcpy(&dst_mask, msg + Filter::DST_MASK_POS, Filter::DST_MASK_LEN);
	len -= Filter::DST_MASK_LEN;

	in_addr_t relay_net;
	int pos = Filter::RPRT_POS;
	for (int i = 0; i < net_num; i++) {
		if (len < sizeof(in_addr_t)) {
			DEBUG_PRINT(2, "Incomplete advice message: rl_net[%d]\n", i);
			return -1;
		}
		memcpy(&relay_net, msg + pos, sizeof(in_addr_t));
		pos += sizeof(in_addr_t);
		len -= sizeof(in_addr_t);
//		net_set.insert(NetUnit(relay_net, time(NULL)));
		net_set.insert(relay_net);
	}

	return 0;
}

#if 0
char *
SimpleRldvr::pack_rl_share(int &len, in_addr_t src_ip, 
						   in_addr_t dst_ip, in_addr_t rly_ip,
						   timeval update, uint32_t delay)
{
	len = RL_SHARE_LEN;	
	char *msg = new char[len];
	
	memset(msg, 0, len);

	cors::PACKET_TYPE type = cors::TYPE_RLY_SHARE;
	memcpy(msg + SR_TYPE_POS, &type, SR_TYPE_LEN);

	uint8_t ver = CUR_VER;
	memcpy(msg + SR_VER_POS, &ver, SR_VER_LEN);

	// skip NUM fields
	// fill timestamp
	timeval tm_now;
	gettimeofday(&tm_now, NULL);
	memcpy(msg + SR_TS_POS, &tm_now, SR_TS_LEN);

	// dst_ip should be in network order
	memcpy(msg + S_SRC_ADDR_POS, &src_ip, S_SRC_ADDR_LEN);
	memcpy(msg + S_DST_ADDR_POS, &dst_ip, S_DST_ADDR_LEN);
	memcpy(msg + S_RLY_ADDR_POS, &rly_ip, S_RLY_ADDR_LEN);

	memcpy(msg + S_DLY_POS, &delay, S_DLY_POS);
	
	return msg;
}

int 
SimpleRldvr::parse_rl_share(const char *msg, int len, in_addr_t &src_ip,
				   			in_addr_t &dst_ip, in_addr_t &rly_ip,
				   			timeval &update, uint32_t &delay)
{
	assert(len >= 2 && msg[SR_TYPE_POS] == cors::TYPE_RLY_SHARE && msg[SR_VER_POS] == CUR_VER);
	len -= (SR_TYPE_LEN + SR_VER_LEN);

	if (len < SR_NUM_LEN) {
		DEBUG_PRINT(2, "Incomplete share message\n");
		return -1;
	}
	len -= SR_NUM_LEN;

	if (len < SR_TS_LEN) {
		DEBUG_PRINT(2, "Incomplete share message: timestamp\n");
		return -1;
	}
	memcpy(&update, msg + SR_TS_POS, SR_TS_LEN);
	len -= SR_TS_LEN;

	if (len < S_SRC_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete share message: src_ip\n");
		return -1;
	}
	memcpy(&src_ip, msg + S_SRC_ADDR_POS, S_SRC_ADDR_LEN);
	len -= S_SRC_ADDR_LEN;

	if (len < S_DST_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete share message: dst_ip\n");
		return -1;
	}
	memcpy(&dst_ip, msg + S_DST_ADDR_POS, S_DST_ADDR_LEN);
	len -= S_DST_ADDR_LEN;

	if (len < S_RLY_ADDR_LEN) {
		DEBUG_PRINT(2, "Incomplete share message: rly_ip\n");
		return -1;
	}
	memcpy(&rly_ip, msg + S_RLY_ADDR_POS, S_RLY_ADDR_LEN);
	len -= S_RLY_ADDR_LEN;

	if (len < S_DLY_LEN) {
		DEBUG_PRINT(2, "Incomplete share message: delay\n");
		return -1;
	}
	memcpy(&delay, msg + S_DLY_POS, S_DLY_LEN);

	return 0;
}

char *
SimpleRldvr::pack_rl_report(int &len, uint16_t src_id, uint16_t dst_id,
					 		uint16_t kn_num, const KnList &kn_list)
{
	if (kn_num > MAX_KN_NUM) {
//		DEBUG_PRINT(2, "Cannot send more than %d relay units in a advice packet\n", MAX_RL_NUM);
		kn_num = MAX_KN_NUM;
	}
	if (kn_num > kn_list.size()) {
		kn_num = kn_list.size();
	}

	len = SR_COMMON_HEADER_LEN + kn_num * sizeof(Knowledge); //12 + kn_num * 8
	char *msg = new char[len];

	memset(msg, 0, len);

	cors::PACKET_TYPE type = cors::TYPE_RLY_REPORT;
	memcpy(msg + SR_TYPE_POS, &type, SR_TYPE_LEN);

	uint8_t ver = CUR_VER;
	memcpy(msg + SR_VER_POS, &ver, SR_VER_LEN);

	memcpy(msg + SR_NUM_POS, &kn_num, SR_NUM_LEN);

	timeval tm_now;
	gettimeofday(&tm_now, NULL);
	memcpy(msg + SR_TS_POS, &tm_now, SR_TS_LEN);

	memcpy(msg + R_SRC_ASN_POS, &src_id, R_SRC_ASN_LEN);
	memcpy(msg + R_DST_ASN_POS, &dst_id, R_DST_ASN_LEN);

	KnList::const_iterator iter;
	int i = 0, pos = R_KNLS_POS;
	for (i = 0, iter = kn_list.begin(); i < kn_num; i++, ++iter) {
		memcpy(msg + pos, &(*iter), sizeof(Knowledge));
		pos += sizeof(Knowledge);
	}

	return msg;
}

int 
SimpleRldvr::parse_rl_report(const char *msg, int len, uint16_t &src_id,
							 uint16_t &dst_id, uint16_t &kn_num, KnList &kn_list)
{
	assert(len >= 2 && msg[SR_TYPE_POS] == cors::TYPE_RLY_REPORT && msg[SR_VER_POS] == CUR_VER);
	len -= (SR_TYPE_LEN + SR_VER_LEN);

	if (len < SR_NUM_LEN) {
		DEBUG_PRINT(2, "Incomplete report message: kn_num\n");
		return -1;
	}
	memcpy(&kn_num, msg + SR_NUM_POS, SR_NUM_LEN);
	len -= SR_NUM_LEN;

	if (len < SR_TS_LEN) {
		DEBUG_PRINT(2, "Incomplete report message: timestamp\n");
		return -1;
	}
	// simply skip it
	len -= SR_TS_LEN;

	if (len < R_SRC_ASN_LEN) {
		DEBUG_PRINT(2, "Incomplete report message: src_id\n");
		return -1;
	}
	memcpy(&src_id, msg + R_SRC_ASN_POS, R_SRC_ASN_LEN);
	len -= R_SRC_ASN_LEN;

	if (len < R_DST_ASN_LEN) {
		DEBUG_PRINT(2, "Incomplete report message: dst_id\n");
		return -1;
	}
	memcpy(&dst_id, msg + R_DST_ASN_POS, R_DST_ASN_LEN);
	len -= R_DST_ASN_LEN;

	Knowledge kn_unit;
	int pos = R_KNLS_POS;
	for (int i = 0; i < kn_num; i++) {
		if (len < sizeof(Knowledge)) {
			DEBUG_PRINT(2, "Incomplete report message: kn_unit[%d]\n", i);
			return -1;
		}
		memcpy(&kn_unit, msg + pos, sizeof(Knowledge));
		pos += sizeof(Knowledge);
		len -= sizeof(Knowledge);
		kn_list.push_back(kn_unit);
	}

	return 0;
}
#endif
