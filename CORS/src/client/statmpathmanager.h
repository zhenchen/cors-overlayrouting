// Copyright (C) 2007 Hui Zhang <zhang.w.hui@gmail.scom>
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

#ifndef __CLIENT_STATMPATHMANAGER_H
#define __CLIENT_STATMPATHMANAGER_H

#include <map>
#include <sstream>
#include <sys/time.h>
#include "simplempathmanager.h"

#define DEFAULT_STAT_ON 2000
#define DEFAULT_STAT_OFF 1000


class StatMpathManager : public MultipathManager {
public:
	enum PARAM_INDEX {STATON, STATOFF};
	list<RelayNode> relaylist;
	StatMpathManager(CorsSocket *sk, vector<string> &param);
	virtual int add_relay(RelayNode & r){}
	virtual int send_packet(char * buff, int nbytes, const sockaddr_in & dst, const Feature & ft, int sk);
  virtual char * parse_data_packet(char * buff, int & bufsize, in_addr_t & saddr, in_port_t & sport, sockaddr_in &peeraddr, int sk, int * flag);
  virtual void process_path_statistics(char * msg, int nbytes, sockaddr_in & peer, int sk);
  virtual bool get_requirement(Requirement & rq);
  virtual void release_resources(int sk);
  
private:
	int stat_on, stat_off;
	vector<RelayNode> goodrelays; // selected relay nodes from relaylist
	vector<RelayNode> badrelays; // removed relay nodes from the goodrelays(high packet loss)
	pthread_mutex_t _map_mutex;
	map<uint32_t, uint32_t> snd_map; // send packet map <relayaddr, send num>
	map<uint32_t, uint32_t> rev_map; // receive packe map <relayaddr, receive num-                                                                                                               >
};

#endif
