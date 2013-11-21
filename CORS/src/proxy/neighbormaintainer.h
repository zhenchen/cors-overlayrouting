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

#ifndef __PROXY_NEIGHBORMAINTAINER_H
#define __PROXY_NEIGHBORMAINTAINER_H

//Interface of Neighbor Maintainer
#include <time.h>
#include <netinet/in.h>
#include <list>
#include <map>
#include "relayunit.h"
#include "../common/corsprotocol.h"

struct Neighbor;
typedef std::map<in_addr_t, Neighbor *> NMap;
typedef std::list<Neighbor *> NList;

class NeighborMaintainer {
 public:
  virtual void start(int sk) = 0;
  virtual time_t get_save_interval() = 0;
  virtual time_t get_probe_interval() = 0;
  virtual bool save() = 0;
  virtual bool probe(int sk) = 0;
  virtual void process_probe_ping(char * msg, int len, sockaddr_in & peeraddr, int sk) = 0;
  virtual void process_probe_pong(char * msg, int len, sockaddr_in & peeraddr, int sk) = 0;
  virtual int fetch_candidate(int num, time_t update, const NetSet &filter, RluList &rl_list) = 0;
 	virtual int get_neighbor_num() = 0;
  virtual int get_neighbor(int index, RelayUnit &rl_unit) = 0;
  virtual int fetch_neighbor_list(NList &nb_list) = 0;
  virtual NList *get_neighbor_list() = 0;

private:
    
};

#endif
