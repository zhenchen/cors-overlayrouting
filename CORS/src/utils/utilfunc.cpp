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

#include "utilfunc.h"
#include <arpa/inet.h>
#include <iostream>
#include <sstream>

using namespace std;

void
split(const string & s, const string & delims, vector<string> & v)
{
  v.clear();
  string::size_type bi, ei;

  bi = s.find_first_not_of(delims);
  while (bi != string::npos) {
    ei = s.find_first_of(delims, bi);
    if (ei == string::npos) {
      ei = s.length();
    }
    v.push_back(s.substr(bi, ei - bi));
    bi = s.find_first_not_of(delims, ei);
  }
}

// judge whether an IP to be an intranet ip, that is one of
// 10.0.0.0/10, 172.16.0.0/12, 192.168.0.0/16
bool
is_intra(in_addr_t addr)
{
  in_addr_t pref, mask;
  inet_pton(AF_INET, "10.0.0.0", &pref);
  inet_pton(AF_INET, "255.0.0.0", &mask);
  if ((addr & mask) == (pref & mask)) return true;
    
  inet_pton(AF_INET, "172.16.0.0", &pref);
  inet_pton(AF_INET, "255.240.0.0", &mask);
  if ((addr & mask) == (pref & mask)) return true;
  
  inet_pton(AF_INET, "192.168.0.0", &pref);
  inet_pton(AF_INET, "255.255.0.0", &mask);
  if((addr & mask) == (pref & mask)) return true;

	inet_pton(AF_INET, "169.254.0.0", &pref);
	inet_pton(AF_INET, "255.255.0.0", &mask);
	if((addr & mask) == (pref & mask)) return true;

	//    cout << (addr & mask) << endl;
  //    cout << (pref & mask) << endl;

  return false;
}

// give the human readable representation of a socket (ip:port)
string
sock2str(int sock)
{
  sockaddr_in skaddrin;
  socklen_t sklen = sizeof(sockaddr_in);
  getsockname(sock, reinterpret_cast<sockaddr *>(&skaddrin), &sklen);
  return sockaddr2str(skaddrin);
}

// convert an socket address into a string
string
sockaddr2str(const sockaddr_in & skaddr)
{
  char buf[24];
  inet_ntop(AF_INET, &skaddr.sin_addr.s_addr, buf, 24);
  stringstream ss;
  ss << buf << ":" << ntohs(skaddr.sin_port) << flush;
  return ss.str();    
}

// convert in_addr_t into a string
string
ip2str(const in_addr_t ip)
{
  char buf[24];
  inet_ntop(AF_INET, &ip, buf, 24);
  stringstream ss;
  ss << buf << flush;
  return ss.str();
}

// convert an array of bytes into string
string
bytes2str(const unsigned char * buf, int len)
{
  stringstream ss;
  for (unsigned int i = 0; i < len; ++i) {
    ss << (unsigned int)buf[i] << "-";
  }
  ss << "\\0";
  return ss.str();
}
