#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <list>
#include "../common/corsprotocol.h"
#include "../utils/utilfunc.h"
#include "../proxy/relayunit.h"

using namespace std;
using namespace cors;

struct Address {
	in_port_t port;
	in_addr_t addr;
	in_addr_t mask;
};

typedef list<Address> AddrList;

enum PacketType {
	INQUIRY,
	FILTER
};

typedef list<PacketType> TaskList;

enum LoadState {
	LOAD_PROXY,
	LOAD_SRC,
	LOAD_DST,
	LOAD_TASK,
	START
};

char *
pack_rl_inquiry(int &len, uint8_t rl_num, in_port_t src_port,
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

char *pack_rl_filter(const char *inquiry_msg,
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

void usage()
{
	cout << "usage: packet_sender config_file" << endl;
}

int load_config(string file_name,
				AddrList &prx_list,
				AddrList &src_list,
				AddrList &dst_list,
				TaskList &task_list)
{
	ifstream ifs(file_name.c_str());
	if (!ifs) {
		cerr << "Unable to config file: " << file_name << endl;
		return -1;
	}

	string line;
	string ip_str, mask_str;
	in_port_t port = 0;
	Address address;
	memset(&address, 0, sizeof(Address));
	LoadState state = START;

	while (!getline(ifs, line).eof()) {
		if (line.compare(0, 5, "proxy") == 0) {
			state = LOAD_PROXY;
		} else if (line.compare(0, 3, "src") == 0) {
			state = LOAD_SRC;
		} else if (line.compare(0, 3, "dst") == 0) {
			state = LOAD_DST;
		} else if (line.compare(0, 4, "task") == 0) {
			state = LOAD_TASK;
		} else {
			if (state == LOAD_TASK) {
				if (line.compare(0, 7, "Inquiry") == 0) 
					task_list.push_back(INQUIRY);
				else
					task_list.push_back(FILTER);
			} else {
				istringstream iss(line);
				iss >> ip_str >> mask_str >> port;
				address.port = htons(port);
				address.addr = inet_addr(ip_str.c_str());
				address.mask = inet_addr(mask_str.c_str());
				switch (state) {
					case LOAD_PROXY:
						prx_list.push_back(address);
						break;
					case LOAD_SRC:
						src_list.push_back(address);
						break;
					case LOAD_DST:
						dst_list.push_back(address);
						break;
					case START:
						break;
				}
			}
		}
	}
	return 0;
}

int init_sock(in_port_t port)
{
	int sk = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sk, (sockaddr *)&local, sizeof(local)) < 0) {
		cerr << "bind error" << endl;
		close(sk);
		return -1;
	}
	return sk;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		usage();
		return -1;
	}
	string conf_file = argv[1];

	AddrList prx_list, src_list, dst_list;
	TaskList task_list;
	int ret = load_config(conf_file, prx_list, src_list, dst_list, task_list);
	if (ret < 0) 
		return -1;
	
	int sk = init_sock(0);
	if (sk < 0)
		return -1;

	NetSet filter;
	filter.insert(inet_addr("219.243.0.0"));
	filter.insert(inet_addr("10.0.0.0"));
	filter.insert(inet_addr("166.111.137.0"));

	AddrList::iterator iter;
	cout << "proxy" << endl;
	for (iter = prx_list.begin(); iter != prx_list.end(); ++iter) {
		in_addr __ip, __mask;
		__ip.s_addr = iter->addr;
		__mask.s_addr = iter->mask;
		cout << inet_ntoa(__ip) << ":"; 
		cout << inet_ntoa(__mask) << ":";
		cout << ntohs(iter->port) << endl;
	}
	cout << "src" << endl;
	for (iter = src_list.begin(); iter != src_list.end(); ++iter) {
		in_addr __ip, __mask;
		__ip.s_addr = iter->addr;
		__mask.s_addr = iter->mask;
		cout << inet_ntoa(__ip) << ":"; 
		cout << inet_ntoa(__mask) << ":";
		cout << ntohs(iter->port) << endl;
	}
	cout << "dst" << endl;
	for (iter = dst_list.begin(); iter != dst_list.end(); ++iter) {
		in_addr __ip, __mask;
		__ip.s_addr = iter->addr;
		__mask.s_addr = iter->mask;
		cout << inet_ntoa(__ip) << ":"; 
		cout << inet_ntoa(__mask) << ":";
		cout << ntohs(iter->port) << endl;
	}

	AddrList::iterator prx_iter = prx_list.begin();
	AddrList::iterator src_iter = src_list.begin();
	AddrList::iterator dst_iter = dst_list.begin();
	TaskList::iterator task_iter = task_list.begin();
	for (uint8_t count = 5; count < task_list.size()+5; count++) {
		int len = 0;
		char *msg;
		if (*task_iter == INQUIRY) {
			cout << "Inquiry" << endl;
			msg = pack_rl_inquiry(len, count,
									src_iter->port,
									src_iter->addr, src_iter->mask,
									dst_iter->addr, dst_iter->mask,
									10, 150);
		} else {
			cout << "Filter" << endl;
			char *inquiry = pack_rl_inquiry(len, count,
											src_iter->port,
											src_iter->addr, src_iter->mask,
											dst_iter->addr, dst_iter->mask,
											10, 150);
			msg = pack_rl_filter(inquiry, len, filter.size(), filter);
			delete[] inquiry;
		}
		
		sockaddr_in proxyaddr;
		memset(&proxyaddr, 0, sizeof(sockaddr_in));
		proxyaddr.sin_family = AF_INET;
		proxyaddr.sin_port = prx_iter->port;
		proxyaddr.sin_addr.s_addr = prx_iter->addr;

		time_t t = time(NULL);
		char *ct = ctime(&t);
		ct[strlen(ct) - 1] = '\0';
		cout << "[" << ct << "]:" << "send to "
			 << sockaddr2str(proxyaddr) << endl;
		cout << "-->: " << bytes2str((unsigned char *)msg, len) << endl;
		
		sendto(sk, msg, len, 0, (sockaddr *)&proxyaddr, sizeof(proxyaddr));
		usleep(1000000);

		delete[] msg;
		++src_iter;
		++dst_iter;
		++prx_iter;
		++task_iter;
		if (src_iter == src_list.end())
			src_iter = src_list.begin();
		if (dst_iter == dst_list.end())
			dst_iter = dst_list.begin();
		if (prx_iter == prx_list.end())
			prx_iter = prx_list.begin();
		if (task_iter == task_list.end())
			task_iter = task_list.begin();
	}
	close(sk);
	return 0;
}
