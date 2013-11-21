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

#ifndef __UTILS_UTILFUNC_H
#define __UTILS_UTILFUNC_H

#include <string>
#include <vector>
#include <netinet/in.h>
#include <sys/types.h>

void split(const std::string & s, const std::string & delims, std::vector<std::string> & v);
bool is_intra(in_addr_t addr);
std::string sock2str(int sock);
std::string sockaddr2str(const sockaddr_in & skaddr);
std::string ip2str(const in_addr_t ip);
std::string bytes2str(const unsigned char * buf, int len);

#endif
