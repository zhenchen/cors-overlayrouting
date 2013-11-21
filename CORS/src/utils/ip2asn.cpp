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

//////////////////////////////////////////////////////////
// ip to asn mapping
// making use of dns service provided by cymru
// (http://asn.cymru.com/)
//////////////////////////////////////////////////////////
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <fstream>
#include <list>
#include <unistd.h>
#include <iostream>
#include "ip2asn.h"

using namespace std;

unsigned short
ip2asn(in_addr_t ip, const string & resolveconf)
{
  const uword QTYPE = htons(16);
  const uword QCLASS = htons(1);
  ifstream ifs(resolveconf.c_str());
  if (!ifs) return 0;
  string line;
  string::size_type bi, ei;
  in_addr_t dnsservip = 0;
  while (getline(ifs, line)) {
    if (line.substr(0, 10) == "nameserver") {
      bi = line.find_first_of("0123456789", 11);
      ei = line.find_last_of("0123456789");
      string ip = line.substr(bi, ei - bi + 1);
      if (inet_pton(AF_INET, ip.c_str(), &dnsservip) > 0) {
        break;
      }
    }
  }
  ifs.close();
  if (dnsservip == 0) return 0;

  sockaddr_in dnsservaddr;
  bzero(&dnsservaddr, sizeof(dnsservaddr));
  dnsservaddr.sin_family = AF_INET;
  dnsservaddr.sin_port = htons(53);
  dnsservaddr.sin_addr.s_addr = dnsservip;

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  DNSMsg msg;
  bzero(&msg, sizeof(msg));
  msg.header.id = htons(3421);
  msg.header.flags = htons(0x0100);
  msg.header.nque = htons(1);

  list<string> appl;
  appl.push_back("origin");
  appl.push_back("asn");
  appl.push_back("cymru");
  appl.push_back("com");

  char buf[24];
  string name = inet_ntop(AF_INET, &ip, buf, 24);
  bi = name.find_first_not_of('.');
  while (bi != string::npos) {
    ei = name.find_first_of('.', bi);
    if (ei == string::npos)
      ei = name.length();
    appl.push_front(name.substr(bi, ei - bi));
    bi = name.find_first_not_of('.', ei);
  }
  
  int offset = 0;
  char termlen;
  for (list<string>::iterator it = appl.begin(); it != appl.end(); ) {
    termlen = (char) it->length();
    msg.data[offset] = termlen;
    ++ offset;
    memcpy(msg.data + offset, it->c_str(), termlen);
    offset += termlen;
    ++ it;
  }
  msg.data[offset++] = 0;

  memcpy(msg.data + offset, &QTYPE, sizeof(QTYPE));
  offset += sizeof(QTYPE);
  memcpy(msg.data + offset, &QCLASS, sizeof(QCLASS));
  offset += sizeof(QCLASS);

  sendto(sock, &msg, sizeof(DNSHeader) + offset, 0, (sockaddr*)&dnsservaddr, sizeof(sockaddr));
  
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);
  struct timeval tv = {2, 0};
  
  int retval = select(sock + 1, &rfds, NULL, NULL, &tv);
  if (FD_ISSET(sock, &rfds)) {
    int len = recvfrom(sock, &msg, 512, 0, NULL, NULL);
    if ((msg.header.id == htons(3421)) && (msg.header.nans > 0)) {
      int pos = 0;
      unsigned char tlen = msg.data[pos];
      while (tlen) {
        if (tlen >= 0xc0) {
	       pos += 2;
	       break;
        } else {
	       tlen = msg.data[pos];
	       pos += tlen + 1;
        }
      }
      pos += sizeof(QTYPE) + sizeof (QCLASS);
      tlen = msg.data[pos];
      while (tlen) {
        if (tlen >= 0xc0) {
	       pos += 2;
	       break;
        } else {
	       tlen = msg.data[pos];
	       pos += tlen + 1;
        }
      }
      if (msg.data[pos+1] == 0x10) { //the QTYPE of answer must be TXT(0x10)
        pos += sizeof(QTYPE) + sizeof(QCLASS) + 4 + 2 + 1; //TTL(4), DATA_LEN(2), DATA_SELF_LEN(1)
        unsigned short asn;
        sscanf(msg.data + pos, "%u", &asn);
        return htons(asn);
      }
    }
  }
  return 0;  
}
