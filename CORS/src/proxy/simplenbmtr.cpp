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
#include <stdexcept>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "simplenbmtr.h"
#include "../utils/utilfunc.h"
#include "../utils/debug.h"
#include "../utils/unpifi.h"
#include "../utils/ip2asn.h"

using namespace std;

const int SimpleNbmtr::TYPE_POS;
const int SimpleNbmtr::TYPE_LEN;
const int SimpleNbmtr::TS_POS;
const int SimpleNbmtr::TS_LEN;
const int SimpleNbmtr::NBNUM_POS;
const int SimpleNbmtr::NBNUM_LEN;
const int SimpleNbmtr::SEQ_POS;
const int SimpleNbmtr::SEQ_LEN;
const int SimpleNbmtr::ADDR_POS;
const int SimpleNbmtr::ADDR_LEN;
const int SimpleNbmtr::PING_HEADER_LEN;
const int SimpleNbmtr::MASK_POS;
const int SimpleNbmtr::MASK_LEN;
const int SimpleNbmtr::ASN_POS;
const int SimpleNbmtr::ASN_LEN;
const int SimpleNbmtr::PONG_HEADER_LEN;
const int SimpleNbmtr::PORT_LEN;


SimpleNbmtr::SimpleNbmtr(vector<string> & param)
    : probe_interval(1), save_interval(60), minsize(60), maxsize(300), numperquery(20), abate_interval(1200)
{
    get_local_interface();
    conf_file = param[CONF_FILE];
    ifstream ifs(conf_file.c_str());
    
    if (!ifs) {
	throw runtime_error("construct SimpleNbmtr failed");
	return;
    }
    string line;
    vector<string> v;
    Neighbor * pn;
    pn = new Neighbor();
    while (getline(ifs, line)) {
       istringstream iss(line);
       iss >> pn->type >> pn->addr >> pn->port >> pn->rtt >> pn->ts >> pn->asn >> pn->mask;
	if ((pn->type < 0) || (pn->addr == 0) || (pn->port == 0) || isself(pn->addr) || is_intra(pn->addr)) {
	    continue;
	}	
	
	nm[pn->addr] = pn;
	nl.push_back(pn);
	
	pn = new Neighbor();
    }
    ifs.close();
    if (nl.empty()) {
	throw runtime_error("construct SimpleNbmtr failed");
	return;
    }

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::constructor" << endl;
    char tmp[16];
    int index = 0;
    for (NMap::iterator it = nm.begin(); it != nm.end(); ++it) {
	cout << "neighbor(" << index << "): ";
	cout << (int)it->second->type << "(type) ";
	cout << inet_ntop(AF_INET, &(it->second->addr), tmp, 16) << "(addr) ";
	cout << ntohs(it->second->port) << "(port) ";
	cout << it->second->rtt << "(rtt) ";
	cout << it->second->ts << "(ts) ";
	cout << it->second->asn << "(asn) ";
	cout << inet_ntop(AF_INET, &(it->second->mask), tmp, 16) << " (mask)" << endl;

	++ index;
    }
#endif
    
    vector<string>::size_type n = param.size();
    
    if (n <= PROBE_INTERVAL) return;
    istringstream iss1(param[PROBE_INTERVAL]);
    iss1 >> probe_interval;

    if (n <= SAVE_INTERVAL) return;
    istringstream iss2(param[SAVE_INTERVAL]);
    iss2 >> save_interval;

    if (n <= MIN_SIZE) return;
    istringstream iss3(param[MIN_SIZE]);
    iss3 >> minsize;

    if (n <= MAX_SIZE) return;
    istringstream iss4(param[MAX_SIZE]);
    iss4 >> maxsize;

    if (n <= NUMBER_PER_QUERY) return;
    istringstream iss5(param[NUMBER_PER_QUERY]);
    int n2c;
    iss5 >> n2c;
    numperquery = (char)n2c;
    
    if (n <= ABATE_INTERVAL) return;
    istringstream iss6(param[ABATE_INTERVAL]);
    iss6 >> abate_interval;
}

void
SimpleNbmtr::start(int sk)
{
    EventQueue * q = EventQueue::Instance();
    q->add_event(new ProbeEvent(time(NULL) + probe_interval, this, sk));
    q->add_event(new SaveEvent(time(NULL) + save_interval, this));
}

bool
SimpleNbmtr::save()
{
    ofstream ofs(conf_file.c_str());
    for (NMap::iterator it = nm.begin(); it != nm.end(); ++it) {
	ofs << it->second->type << " "
	    << it->second->addr << " "
	    << it->second->port << " "
	    << it->second->rtt << " "
	    << it->second->ts << " "
	    << it->second->asn << " "
	    << it->second->mask << endl;
    }

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::save" << endl;
    char tmp[16];
    int index = 0;
    for (NMap::iterator it = nm.begin(); it != nm.end(); ++it) {
	cout << "neighbor(" << index << "): ";
	cout << (int)it->second->type << "(type) ";
	cout << inet_ntop(AF_INET, &(it->second->addr), tmp, 16) << "(addr) ";
	cout << ntohs(it->second->port) << "(port) ";
	cout << it->second->rtt << "(rtt) ";
	cout << it->second->ts << "(ts) ";
	cout << it->second->asn << "(asn) ";
	cout << inet_ntop(AF_INET, &(it->second->mask), tmp, 16) << " (mask)" << endl;

	++ index;
    }

#endif
    
    return true;
}

bool
SimpleNbmtr::probe(int sk)
{
    Neighbor * pn = nl.front();

    sockaddr_in peeraddr;
    peeraddr.sin_family = AF_INET;
    
    //type of packet, first byte
    buff[TYPE_POS] = cors::TYPE_PROBE_PING; 

    timeval tv;
    gettimeofday(&tv, NULL);
    memcpy(buff + TS_POS, &tv, TS_LEN);

    //number of neighbors required, second byte
    int n = nm.size();
    if (n < minsize) {
	buff[NBNUM_POS] = numperquery;
	if (++nl.begin() != nl.end()) {    //send request to last probed neighbor if possible
	   buff[SEQ_POS] = nl.back()->seq;
	    ++ nl.back()->seq;
	    if (nl.back()->seq >= 250) {
	       nl.back()->seq = 0;
	    }
	    memcpy(buff + ADDR_POS, &(nl.back()->addr), ADDR_LEN);	    
	    peeraddr.sin_addr.s_addr = nl.back()->addr;
	    peeraddr.sin_port = nl.back()->port;
	    //sendto(sk, buff, PING_HEADER_LEN, MSG_DONTWAIT, (sockaddr*)&peeraddr, sizeof(sockaddr));
	    sendto(sk, buff, PING_HEADER_LEN, 0, (sockaddr*)&peeraddr, sizeof(sockaddr));
	}
    } else {
	buff[NBNUM_POS] = 0;
    }

    //destination's addr, used by the probed neighbor to determine its mask of that interface
    //third ~ sixth bytes
    buff[SEQ_POS] = pn->seq;
    ++ pn->seq;
    if (pn->seq >= 250) {
	pn->seq = 0;
    }
    memcpy(buff + ADDR_POS, &(pn->addr), ADDR_LEN);
    peeraddr.sin_addr.s_addr = pn->addr;
    peeraddr.sin_port = pn->port;
    //sendto(sk, buff, PING_HEADER_LEN, MSG_DONTWAIT, (sockaddr*)&peeraddr, sizeof(sockaddr));
    sendto(sk, buff, PING_HEADER_LEN, 0, (sockaddr*)&peeraddr, sizeof(sockaddr));

    pn->ts = time(NULL);
    if ((pn->ts - nl.back()->ts >= abate_interval)
	&& (nl.back()->type != Neighbor::ORACLE)
	&& (nm.size() > 1)) {
	nm.erase(nm.find(nl.back()->addr));
	delete nl.back();
	nl.pop_back();
    }    
    nl.push_back(pn);
    nl.pop_front();

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::probe ("
	 << sockaddr2str(peeraddr) << ")" << endl;
    cout << "-->: " << bytes2str((unsigned char *)buff, PING_HEADER_LEN) << endl;
#endif

    return true;
}

void
SimpleNbmtr::process_probe_ping(char * msg, int len, sockaddr_in & peeraddr, int sk)
{
    if (nm.find(peeraddr.sin_addr.s_addr) == nm.end()) {
	//reverse learning
	Neighbor * pn = new Neighbor(Neighbor::PEER, peeraddr.sin_addr.s_addr, peeraddr.sin_port, time(NULL));	
	nm[peeraddr.sin_addr.s_addr] = pn;
	nl.push_front(pn);
    }

    memcpy(buff, msg, len);

    //change packet type
    buff[TYPE_POS] = cors::TYPE_PROBE_PONG;
    
    //get #neighbors required
    unsigned char n = buff[NBNUM_POS];

    //increase seq
    ++ buff[SEQ_POS];

    //fill in the mask according to coming interface
    in_addr_t addr;
    uint16_t tmpns;
    memcpy(&addr, buff + ADDR_POS, ADDR_LEN);
    vector<NetInterface>::iterator it = ifvec.begin();
    for ( ; it != ifvec.end(); ++ it) {
	if (it->addr == addr) {
	    memcpy(buff + MASK_POS, &(it->mask), MASK_LEN);
	    tmpns = htons(it->asn);
	    memcpy(buff + ASN_POS, &tmpns, ASN_LEN);
	    break;
	}
    }
    if (it == ifvec.end()) {
	memset(buff + MASK_POS, 0xFF, MASK_LEN);
	memset(buff + ASN_POS, 0, ASN_LEN);
    }

    //provide neighbors
    NList::reverse_iterator rit = nl.rbegin();
    unsigned char m = 0;
    char * pos = buff + PONG_HEADER_LEN;
    while ((rit != nl.rend()) && (m < n)) {
	if ((*rit)->addr != peeraddr.sin_addr.s_addr) {
	    *pos = (*rit)->type;
	    pos += TYPE_LEN;
	    memcpy(pos, &((*rit)->addr), ADDR_LEN);
	    pos += ADDR_LEN;
	    memcpy(pos, &((*rit)->port), PORT_LEN);
	    pos += PORT_LEN;
	    memcpy(pos, &((*rit)->mask), MASK_LEN);
	    pos += MASK_LEN;
	    tmpns = htons((*rit)->asn);
	    memcpy(pos, &tmpns, ASN_LEN);
	    pos += ASN_LEN;
	    
	    ++ m;
	}
	++ rit;
    }

    //reset real #neighbor
    buff[NBNUM_POS] = m;

    //send pong msg
    //sendto(sk, buff, pos - buff, MSG_DONTWAIT, (sockaddr*)&peeraddr, sizeof(sockaddr));
    sendto(sk, buff, pos - buff, 0, (sockaddr*)&peeraddr, sizeof(sockaddr));

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::process_probe_ping ("
	 << sockaddr2str(peeraddr) << ")" << endl;
    cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
    cout << "-->: " << bytes2str((unsigned char *)buff, pos - buff) << endl;
#endif
}

void
SimpleNbmtr::process_probe_pong(char * msg, int len, sockaddr_in & peeraddr, int sk)
{
    in_addr_t addr;
    memcpy(&addr, msg + ADDR_POS, ADDR_LEN); //get probed neighbor's addr

    NMap::iterator it = nm.find(peeraddr.sin_addr.s_addr);
    //check if this pong message is expected
    int flag = 0;
    if (addr != peeraddr.sin_addr.s_addr) {
	flag |= 1;
    }
    if (it == nm.end()) {
	flag |= 2;
    } else if (it->second->seq != (unsigned char)msg[SEQ_POS]) {
	flag |= 4;
    }
    if (flag) {
#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::received illegal pong message ("
	 << flag << ")" << endl;
    cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
#endif	
	return;
    }
    
    //calculate rtt
    timeval tvsent, tvnow;
    memcpy(&tvsent, msg + TS_POS, TS_LEN);
    gettimeofday(&tvnow, NULL);
    double rtt = tvnow.tv_usec - tvsent.tv_usec;
    rtt /= 1000;
    rtt += (tvnow.tv_sec - tvsent.tv_sec) * 1000;

    Neighbor * pn = it->second;
    //update the probed neighbor's rtt and ts
    pn->rtt = rtt;
    pn->ts = time(NULL);

    //get & update the probed neighbor's mask and asn
    memcpy(&pn->mask, msg + MASK_POS, MASK_LEN);
    uint16_t asn;
    memcpy(&asn, msg + ASN_POS, ASN_LEN);    
    pn->asn = ntohs(asn);

    //push the novel neighbors in front of nl
    char * pos = msg + PONG_HEADER_LEN;
    char type;
    in_addr_t mask;
    in_port_t port;
    for (unsigned char i = 0; i < (unsigned char)msg[NBNUM_POS]; ++i) {	
	memcpy(&addr, pos + TYPE_LEN, ADDR_LEN);
	if (nm.find(addr) == nm.end()) {
	    type = *pos;
	    pos += (TYPE_LEN + ADDR_LEN);
	    memcpy(&port, pos, PORT_LEN);
	    pos += PORT_LEN;
	    memcpy(&mask, pos, MASK_LEN);
	    pos += MASK_LEN;
	    memcpy(&asn, pos, ASN_LEN);
	    pos += ASN_LEN;
	    pn = new Neighbor(type, addr, port, time(NULL), mask, asn);
	    nm[addr] = pn;
	    nl.push_front(pn);
	}
    }
    
#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    time_t t = time(NULL);
    char * ct = ctime(&t);
    ct[strlen(ct) - 1] = '\0';
    cout << "[" << ct << "]: " << "SimpleNbmtr::process_probe_pong ("
	 << sockaddr2str(peeraddr) << ")" << endl;
    cout << "<--: " << bytes2str((unsigned char *)msg, len) << endl;
#endif
}

int
SimpleNbmtr::fetch_candidate(int num, time_t update, const NetSet &filter, RluList &rl_list)
{
	time_t tm_now = time(NULL);
	NList::reverse_iterator iter = nl.rbegin();
	int i = 0;
	for ( ; i < num && iter != nl.rend(); i++, ++iter) {
//		if (tm_now - (*iter)->ts <= update) {
			rl_list.push_back(RelayUnit((*iter)->addr,
										(*iter)->mask,
										(*iter)->port,
										(*iter)->ts,
										0));
//		}
	}
	return i + 1;
}

int
SimpleNbmtr::fetch_neighbor_list(NList &nb_list)
{
	nb_list.assign(nl.begin(), nl.end());
	return nb_list.size();
}

int
SimpleNbmtr::get_neighbor(int index, RelayUnit &rl_unit)
{
	if (index >= nl.size()) index = nl.size() - 1;
	NList::reverse_iterator iter = nl.rbegin();
	for (int i = 0; i < index; i++) 
		++iter;
	rl_unit.addr = (*iter)->addr;
	rl_unit.port = (*iter)->port;
	return 0;
}

// get information of local interface except the loopback and tunnel ones
// filter out aliases
void
SimpleNbmtr::get_local_interface()
{
  ifvec.clear();
  struct ifi_info *ifi, *ifihead;
  struct ifreq ifr;
  NetInterface ni;
  for (ifihead = ifi = Get_ifi_info(AF_INET, 0); ifi != NULL; ifi = ifi->ifi_next) {
    if (ifi->ifi_addr == NULL) continue;
    ni.name = ifi->ifi_name; 
    ni.addr = reinterpret_cast<sockaddr_in *>(ifi->ifi_addr)->sin_addr.s_addr;

    if (ifi->ifi_mask == NULL)
	    ni.mask = 0xFFFFFFFF;
    else
	    ni.mask = reinterpret_cast<sockaddr_in *>(ifi->ifi_mask)->sin_addr.s_addr;
    ni.mtu = ifi->ifi_mtu;

    if ((ni.name != "lo") && (ni.name != "tun") && (!is_intra(ni.addr))) {
      //mapping ip to asn
      ni.asn = ip2asn(ni.addr);
      ifvec.push_back(ni);
    }

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    char buf[24];	
    cout<<"*****Network Interface*****"<<endl;
    cout<<"\tname:"<<ni.name<<endl;
    inet_ntop(AF_INET, &ni.addr, buf, 24);
    cout<<"\taddr:"<<ntohl(ni.addr)<<"("<<buf<<")"<<endl;
    inet_ntop(AF_INET, &ni.mask, buf, 24);
    cout<<"\tmask:"<<ntohl(ni.mask)<<"("<<buf<<")"<<endl;
    cout<<"\tmtu:"<<ni.mtu<<endl<<endl;	
#endif
  }

#if (defined(NBMTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
  cout << "Ifvec:" << endl;
  for (vector<NetInterface>::iterator it = ifvec.begin(); it != ifvec.end(); ++it) {
    char buf[24];	
    cout<<"*****Network Interface*****"<<endl;
    cout<<"\tname:"<<it->name<<endl;
    inet_ntop(AF_INET, &it->addr, buf, 24);
    cout<<"\taddr:"<<ntohl(it->addr)<<"("<<buf<<")"<<endl;
    inet_ntop(AF_INET, &it->mask, buf, 24);
    cout<<"\tmask:"<<ntohl(it->mask)<<"("<<buf<<")"<<endl;
    cout<<"\tmtu:"<<it->mtu<<endl<<endl;
  }
#endif
}

bool
SimpleNbmtr::isself(in_addr_t addr)
{
  for (vector<NetInterface>::iterator it = ifvec.begin(); it != ifvec.end(); ++ it) {
    if (it->addr == addr) {
	    return true;
    }	
  }
  return false;
}
