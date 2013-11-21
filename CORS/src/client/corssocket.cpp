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

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "corssocket.h"
#include "rsltrfactory.h"
#include "mmgrfactory.h"
#include "../common/corsprotocol.h"
#include "../utils/utilfunc.h"
#include "../utils/debug.h"
#include "../utils/unpifi.h"
#include "../utils/ip2asn.h"

using namespace cors;
using namespace std;

const string INTERFACE = "eth0";

/** constructor 
 * \param sk descriptor of a socket, must be created and binded already before
 * \param rsparams parameters of RelaySelector, a string vector
 * \param mmparams parameters of MultipathManager, a string vector
 */
CorsSocket::CorsSocket(int sk, vector<string> & rsparams, vector<string> & mmparams)
  : sock(sk), dstaddr(0), dstport(0), dstmask(0), dstasn(0), last_query_time(0)
{
  // pthread_mutex_init(&cors_lock, NULL);
  get_local_interface();
  vector<NetInterface>::iterator it = ifvec.begin();
  for (; it != ifvec.end(); ++it) {
    if (it->name == INTERFACE) {
      srcaddr = it->addr;
      srcmask = it->mask;
      break;
    }
  }
  if (it == ifvec.end()) {
    DEBUG_PRINT(3, "Cannot find eth0, using the first net interface instead\n");	
    srcaddr = ifvec[0].addr;
    srcmask = ifvec[0].mask;
  }

  sockaddr_in name;
  socklen_t namelen = sizeof(name);
  getsockname(sk, (sockaddr *)&name, &namelen);
  srcport = name.sin_port;

  srcasn = ip2asn(srcaddr);

  rlysltr = RsltrFactory::getInstance()->create_rsltr(this, rsparams);
  mpthmgr = MmgrFactory::getInstance()->create_mmgr(this, mmparams); 
}

/** destructor */
CorsSocket::~CorsSocket()
{
  if (rlysltr) delete rlysltr;
  if (mpthmgr) delete mpthmgr;
}


/** CorsSocket initialization
 * \param rq expected Requirement of overlay path, for path selection
 * \param daddr destination address of the communication
 * \param dport destination port of the communication
 */
void
CorsSocket::init(const Requirement & rq, in_addr_t daddr, in_port_t dport)
{
  int ret;
  dstaddr = daddr;
  dstport = dport;

  dstasn = ip2asn(dstaddr);
  dstmask = inet_addr("255.255.255.255");

  rlysltr->set_requirement(rq);
  rlysltr->send_rl_inquiry(rq, sock);
  last_query_time = time(NULL);
  // ret=pthread_create(&cycle_tid,NULL,rlysltr->cycle_ping,rlysltr);
  // rlysltr->send_candidates_request(rq, sock);
}

/** send a packet
 * \param buff pointer to the buffer of packet
 * \param nbytes length of the packet to be sent
 * \param daddr destination ip address of the packet
 * \param dport destination port of the packet
 * \param feature packet Feature
 * \return the length sent out
 */
int
CorsSocket::sendto(char * buff, int nbytes, in_addr_t daddr, in_port_t dport, const Feature & feature)
{
  // struct timeval tm_start, tm_end;
  // gettimeofday(&tm_start, NULL);
  // pthread_mutex_lock(&cors_lock);
  // timeval tm_start, tm_end, tm_used;
  // gettimeofday(&tm_start, NULL);
  // DEBUG_PRINT(4, "enter sendto() at %d.%06d\n", tm_start.tv_sec, tm_start.tv_usec);
  sockaddr_in dst;
  dst.sin_family = AF_INET;
  dst.sin_addr.s_addr = dstaddr;
  dst.sin_port = dstport;
  // gettimeofday(&tm_end, NULL);
  // DEBUG_PRINT(4, "before send_packet() at %d.%06d\n", tm_end.tv_sec, tm_end.tv_usec);
  // tm_used.tv_sec = tm_end.tv_sec - tm_start.tv_sec;
  // tm_used.tv_usec = tm_end.tv_usec - tm_start.tv_usec;
  // if (tm_used.tv_usec < 0) {
  //   tm_used.tv_usec += 1000000;
  //   tm_used.tv_sec -= 1;
  // }
  // DEBUG_PRINT(4, "time used before send_packet() = %d.%06d\n", tm_used.tv_sec, tm_used.tv_usec);
  if ((dstaddr == daddr) && (dstport == dport)) {
    mpthmgr->send_packet(buff, nbytes, dst, feature, sock);
  } else {
    ::sendto(sock, buff, nbytes, 0, (sockaddr*)&dst, sizeof(dst));
  }

  // gettimeofday(&tm_start, NULL);
  // DEBUG_PRINT(4, "Do more thing start at %d.%06d\n", tm_start.tv_sec, tm_start.tv_usec)
  time_t now = time(NULL);
  if (now - last_query_time > 3) {
    Requirement rq;
    rlysltr->fill_requirement(rq);
    if (!mpthmgr->get_requirement(rq)) {
      rlysltr->send_rl_inquiry(rq, sock);
      last_query_time = now;
      // rlysltr->send_candidates_request(rq, sock);
    }
  }
  // gettimeofday(&tm_end, NULL);
  // DEBUG_PRINT(4, "Do more thing end at %d.%06d\n", tm_end.tv_sec, tm_end.tv_usec)
  // tm_used.tv_sec = tm_end.tv_sec - tm_start.tv_sec;
  // tm_used.tv_usec = tm_end.tv_usec - tm_start.tv_usec;
  // if (tm_used.tv_usec < 0) {
  //   tm_used.tv_usec += 1000000;
  //   tm_used.tv_sec -= 1;
  // }
  // DEBUG_PRINT(4, "#### in more thing time_used = %d.%06d\n", tm_used.tv_sec, tm_used.tv_usec);
  // gettimeofday(&tm_end, NULL);
  // DEBUG_PRINT(4, "#### exit sendto() at %d.%06d\n", tm_end.tv_sec, tm_end.tv_usec);
  // pthread_mutex_unlock(&cors_lock);
  // gettimeofday(&tm_end, NULL);
  // cout << "time_used sendto = " << tm_end.tv_usec - tm_start.tv_usec << endl;
  // DEBUG_PRINT(4, "time_used sendto = %d us\n", tm_end.tv_usec - tm_start.tv_usec);
  return nbytes;
}

/** receive a packet
 * \param buff pointer to buffer where store the packet
 * \param nbytes filled with number of bytes received
 * \param saddr filled with source ip address of the packet (not address of relay node)
 * \param sport filled with source port of the packet (not port on relay node)
 * \param flag filled with a mark indicating direct or indirect path (if flag is not NULL)
 * \return number of bytes received
 */
int 
CorsSocket::recvfrom(char * buff, int & nbytes, in_addr_t & saddr, in_port_t & sport, int * flag)
{
  // pthread_mutex_lock(&cors_lock);
  char * data;
  sockaddr_in peeraddr;
  socklen_t socklen = sizeof(peeraddr);
  int len = ::recvfrom(sock, msg, sizeof(msg) / sizeof(char), 0, (sockaddr*)&peeraddr, &socklen);
  if (len > 0) {
    switch (msg[0]) {
    case TYPE_DATA:
      data = mpthmgr->parse_data_packet(msg, len, saddr, sport, peeraddr, sock, flag);
      memcpy(buff, data, len);
      nbytes = len;
      break;
    case TYPE_STAT:
      mpthmgr->process_path_statistics(msg, len, peeraddr, sock);
      break;
    case TYPE_FW_PROBE:
    case TYPE_FW_FORWARD:
      rlysltr->process_fw_forward(msg, len, peeraddr, sock);
      break;
    case TYPE_FW_REPLY:
      rlysltr->process_fw_reply(msg, len, peeraddr, sock);
      break;
    case TYPE_BW_FORWARD:
      rlysltr->process_bw_forward(msg, len, peeraddr, sock);
      break;
    case TYPE_BW_REPLY:
      rlysltr->process_bw_reply(msg, len, peeraddr, sock);
      break;
    case TYPE_RLY_ADVICE:
      rlysltr->process_rl_advice(msg, len, peeraddr, sock);
      break;
    }
  }
  // pthread_mutex_unlock(&cors_lock);
  return nbytes;
}

/** release resources, destruct component objects
 */
void
CorsSocket::close()
{
  rlysltr->release_resources(sock);
  mpthmgr->release_resources(sock);

  delete rlysltr;
  rlysltr = NULL;
  delete mpthmgr;
  mpthmgr = NULL;
}

in_addr_t
CorsSocket::get_default_mask(in_addr_t ip)
{
  // calculate mask from ip class (A, B, C)
  if (ip & 0x80 == 0) {
    return inet_addr("255.0.0.0");		// Class A mask
  } else if (ip & 0x40 == 0) {
    return inet_addr("255.255.0.0");	// Class B
  } else {
    return inet_addr("255.255.255.0");	// Class C
  }
}

void
CorsSocket::get_local_interface()
{
  DEBUG_PRINT(3, "Entering get_local_interface()\n");
  ifvec.clear();
  struct ifi_info *ifi, *ifihead;
  struct ifreq ifr;
  NetInterface ni;
  for (ifihead = ifi = Get_ifi_info(AF_INET, 0); ifi != NULL; ifi = ifi->ifi_next) {
    if (ifi->ifi_addr == NULL) continue;
    ni.name = ifi->ifi_name;
    ni.addr = reinterpret_cast<sockaddr_in *>(ifi->ifi_addr)->sin_addr.s_addr;
    if (ifi->ifi_mask == NULL)
      ni.mask = 0xffffffff;
    else
      ni.mask = reinterpret_cast<sockaddr_in *>(ifi->ifi_mask)->sin_addr.s_addr;
    ni.mtu = ifi->ifi_mtu;

    if ((ni.name != "lo") && (ni.name != "tun") && (!is_intra(ni.addr))) {
      ifvec.push_back(ni);
    }
#if (defined(RSLTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
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

#if (defined(RSLTR_INTEST) && defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
  cout << "Ifvec:" << endl;
  /*map<string, NetInterface>::iterator it = ifmap.begin();
  for (; it != ifmap.end(); ++it) {
    char buf[24]; 
    cout << "*****Network Interface*****" << endl;
    cout << "\tname: " << it->second.name << endl;
    inet_ntop(AF_INET, &it->second.addr, buf, 24);
    cout << "\taddr: " << ntohl(it->second.addr)
         << "(" << buf << ")" << endl;
    inet_ntop(AF_INET, &it->second.mask, buf, 24);
    cout << "\tmask: " << ntohl(it->second.mask)
         << "(" << buf << ")" << endl;
    cout << "\tmtu: " << it->second.mtu << endl << endl; 
  }*/
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
