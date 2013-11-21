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

#ifndef __PROXY_RELAYUNIT_H
#define __PROXY_RELAYUNIT_H

#include <list>
#include <set>

const int DEFAULT_TIMEOUT = 3;

struct RelayUnit {
	in_addr_t 	addr;
	in_addr_t 	mask;
	in_port_t 	port;
	uint8_t		timeout; 
	uint8_t		reserved; 
	time_t 		update;
	uint32_t 	delay;
	RelayUnit() {
		memset(this, 0, sizeof(RelayUnit));
	}
	RelayUnit(in_addr_t ad, in_addr_t ma, in_port_t po, time_t upd, uint32_t dly) 
		: addr(ad), mask(ma), port(po), update(upd), delay(dly) {
		timeout = DEFAULT_TIMEOUT;
		reserved = 0;
	}
};

typedef std::list<RelayUnit> RluList;
typedef std::set<in_addr_t> NetSet;

#endif
