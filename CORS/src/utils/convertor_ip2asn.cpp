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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include "ip2asn.h"

using namespace std;

int
main(int argc, char * argv[])
{
  string resolveconf = argv[1];
  cout << "resolv.conf=" << resolveconf << endl;
  puts("Input IP in dotted format:");
  char buff[32];
  unsigned short asn;
  in_addr_t addr;
  while (gets(buff)) {
    inet_pton(AF_INET, buff, &addr);
    asn = ntohs(ip2asn(addr, resolveconf));
    cout << "asn=" << asn << endl;
  }
}
