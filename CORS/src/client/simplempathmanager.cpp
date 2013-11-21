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

#pragma pack(1)
#include "simplempathmanager.h"
#include "../utils/utilfunc.h"
#include <iomanip>

/** header of the direct data packet */
typedef struct Directdataheader{
   unsigned type:8;
   unsigned ver:4;
   unsigned direct:1;
   unsigned control:3;
   unsigned reserve:8;
}Directdataheader;

/** header of the indirect date packet */
typedef struct Indirectdataheader{
   Directdataheader directheader;
   uint16_t port;
   uint32_t ip;
}Indirectdataheader;

	
int SimpleMpathManager::send_packet(char * buff, int nbytes, 
	const sockaddr_in & dst, const Feature & ft, int sk) {
#if (defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 6))
  struct timeval tm_start, tm_end;
	gettimeofday(&tm_start, NULL);
  cout << "enter send_packet() at " << tm_start.tv_sec
       << "." << setw(6) << setfill('0') << tm_start.tv_usec << endl;
#endif

	static int cur_channel = 0;		
	
	char sbuf[CORS_MAX_MSG_SIZE];
	Directdataheader *Dheader;
	Indirectdataheader *Indheader;
	struct sockaddr_in send_addr;
	memcpy((void*)&send_addr, (void*)(&dst), sizeof(struct sockaddr_in));
		
#if (defined(DEBUG_ENABLED))
  int seq = 0;
  memcpy(&seq, buff + 2, 2);
  seq = ntohs(seq);
#endif
	
  if(ft.significance == IMPORTANCE){
		//mesh sending
		Dheader = (Directdataheader*)(sbuf);
		Dheader->type = TYPE_DATA;
		Dheader->ver = VERSION;
		Dheader->direct = TRUE;
		Dheader->control = ZERO;
		memcpy(sbuf+sizeof(Directdataheader), buff, nbytes);
#ifdef ENABLE_LOSS
    int pass = rand() % 10000;
    if (pass >= LOSSRATE) {
      sendto(sk, (void*)sbuf, nbytes+sizeof(Directdataheader), 0, (struct sockaddr *)&send_addr, sizeof(struct sockaddr));
    }
#else
		sendto(sk, (void*)sbuf, nbytes+sizeof(Directdataheader), 0, 
			(struct sockaddr *)&send_addr, sizeof(struct sockaddr));	
#endif
		DEBUG_PRINT(4, "Direct mesh sendto %s\n", sockaddr2str(send_addr).c_str());
    DEBUG_PRINT(4, "send seqnum = %d\n", seq);
		
		Indheader=(Indirectdataheader*)(sbuf);
		Indheader->directheader.type = TYPE_DATA;
		Indheader->directheader.ver = VERSION;
		Indheader->directheader.direct= FALSE;
		Indheader->directheader.control = ZERO;
		Indheader->ip = dst.sin_addr.s_addr;
		Indheader->port = dst.sin_port;	
		memcpy(sbuf+sizeof(Indirectdataheader), buff, nbytes);
		
		for(int i=0; i<goodrelays.size();i++) {
			send_addr.sin_addr.s_addr = goodrelays.at(i).relay.addr;
			send_addr.sin_port = goodrelays.at(i).relay.port;
#ifdef ENABLE_LOSS
      int pass = rand() % 10000;
      if (pass >= LOSSRATE) {
			  sendto(sk, sbuf, nbytes+sizeof(Indirectdataheader), 0, 
				  (struct sockaddr *)&send_addr, sizeof(struct sockaddr)); 
      }
#else
			sendto(sk, sbuf, nbytes+sizeof(Indirectdataheader), 0, 
				(struct sockaddr *)&send_addr, sizeof(struct sockaddr));
#endif
			DEBUG_PRINT(4, "Indirect mesh sendto %s\n", sockaddr2str(send_addr).c_str());
      DEBUG_PRINT(4, "send seqnum = %d\n", seq);
			// DEBUG_PRINT(4, "indirect packet: %s\n", bytes2str((unsigned char*)sbuf, nbytes + sizeof(Indirectdataheader)).c_str());
      // cout << "relay addr is " << sockaddr2str(send_addr) << endl;
      // cout << "indirect packet: " << bytes2str((unsigned char *)sbuf, nbytes + sizeof(Indirectdataheader)) << endl;
		}
	} else if (ft.significance == NORMAL){
		//round robin sendig
		cur_channel = (cur_channel+1)%(goodrelays.size()+1);
		if(cur_channel == goodrelays.size()) {
			Dheader = (Directdataheader*)(sbuf);
			Dheader->type = TYPE_DATA;
			Dheader->ver = VERSION;
			Dheader->direct = TRUE;
			Dheader->control = ZERO;
			memcpy(sbuf+sizeof(Directdataheader), buff, nbytes);
			sendto(sk, (void*)sbuf, nbytes+sizeof(Directdataheader), 0, 
				(struct sockaddr *)&send_addr, sizeof(struct sockaddr));
			DEBUG_PRINT(4, "Direct round robin sendto %s\n", sockaddr2str(send_addr).c_str());
		}	else {
			Indheader = (Indirectdataheader*)(sbuf);
			Indheader->directheader.type = TYPE_DATA;
			Indheader->directheader.ver = VERSION;
			Indheader->directheader.direct = FALSE;
			Indheader->directheader.control = ZERO;
			Indheader->ip = dst.sin_addr.s_addr;
			Indheader->port = dst.sin_port;	

			send_addr.sin_addr.s_addr = goodrelays.at(cur_channel).relay.addr;
			send_addr.sin_port = goodrelays.at(cur_channel).relay.port;
			sendto(sk, sbuf, nbytes+sizeof(Indirectdataheader), 0, 
					(struct sockaddr *)&send_addr, sizeof(struct sockaddr));
			DEBUG_PRINT(4, "Indirect round robin sendto %s\n", sockaddr2str(send_addr).c_str());
      // cout << "relay addr is " << sockaddr2str(send_addr) << endl;
      // cout << "indirect packet: " << bytes2str((unsigned char *)sbuf, nbytes + sizeof(Indirectdataheader)) << endl;
		}
	}
#if (defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 6))
	gettimeofday(&tm_end, NULL);
  cout << "exit send_packet() at " << tm_end.tv_sec
       << "." << setw(6) << setfill('0') << tm_end.tv_usec << endl;
	cout << "time_used = " << tm_end.tv_usec - tm_start.tv_usec << endl;
#endif
  // DEBUG_PRINT(4, "time_used = %d\n", tm_end.tv_usec - tm_start.tv_usec);
	return nbytes;
}


char * 
SimpleMpathManager::parse_data_packet(char * buff, int & bufsize, 
	in_addr_t & saddr, in_port_t & sport, sockaddr_in &peeraddr, int sk,
  int *flag) {
	struct sockaddr_in from_in;
	Indirectdataheader	*Indheader;
	// DEBUG_PRINT(4, "from address %s\n", sockaddr2str(peeraddr).c_str());
  // cout << "receive packet: " << bytes2str((unsigned char *)buff, bufsize) << endl;

	Indheader=(Indirectdataheader *)(buff);
	if(Indheader->directheader.ver > VERSION) {
		DEBUG_PRINT(3, "Please update your CORS API!\n");
		return NULL;
	}
	else if(Indheader->directheader.direct == TRUE) {
		DEBUG_PRINT(4, "Direct recvfrom %s\n", sockaddr2str(peeraddr).c_str());
		buff = buff + sizeof(Directdataheader);
		bufsize = bufsize - sizeof(Directdataheader);
		sport = peeraddr.sin_port;
		saddr = peeraddr.sin_addr.s_addr;
#if (defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    int seq = 0;
    memcpy(&seq, buff+2, 2);
    seq = ntohs(seq);
    cout << "Recv seqnum = " << seq << endl;
#endif
    if (flag != NULL)
      *flag = 0;   // indication of direct-path recv
		return buff;
	} else {
		DEBUG_PRINT(4, "Indirect recvfrom %s\n", sockaddr2str(peeraddr).c_str());
		memcpy(&sport, buff + PORT_OFFSET, sizeof(sport));
		memcpy(&saddr, buff + ADDR_OFFSET, sizeof(saddr));
		buff = buff + sizeof(Indirectdataheader);
		bufsize = bufsize - sizeof(Indirectdataheader);
		sockaddr_in source;
		memset(&source, 0, sizeof(source));
		source.sin_family = AF_INET;
		source.sin_port = sport;
		source.sin_addr.s_addr = saddr;
		DEBUG_PRINT(4, "source addr %s\n", sockaddr2str(source).c_str());
#if (defined(DEBUG_ENABLED) && (DEBUG_LEVEL >= 4))
    int seq = 0;
    memcpy(&seq, buff+2, 2);
    seq = ntohs(seq);
    cout << "Recv seqnum = " << seq << endl;
#endif
    if (flag != NULL)
      *flag = 1;
		return buff;
	}
}
    

bool SimpleMpathManager::get_requirement(Requirement & rq) {
	int gain_bw=0;
	vector<RelayNode>::iterator good_iter;
	list<RelayNode>::iterator list_iter;
	relaylist.clear();
	(psk->get_rlysltr())->get_relaylist(relaylist);
	
	if(relaylist.empty())	return false;
	// check the relay nodes in the goodrelays
	for(good_iter=goodrelays.begin(); good_iter!=goodrelays.end();) {
		for(list_iter=relaylist.begin(); list_iter!=relaylist.end();list_iter++) {
			if(good_iter->relay.addr==list_iter->relay.addr) {
				gain_bw += (good_iter)->bandwidth;
				good_iter++;
				break;
			}
		}
		if(list_iter==relaylist.end()){
			goodrelays.erase(good_iter);
		}
	}		
	if(gain_bw >= rq.bandwidth && goodrelays.size()>=rq.relaynum)
    return true;	
	
	// add relays into the goodrelays from the relaylist
	for(list_iter=relaylist.begin(); list_iter!=relaylist.end(); list_iter++) {
		for(good_iter=goodrelays.begin(); good_iter!=goodrelays.end();good_iter++) {
			if(good_iter->relay.addr==list_iter->relay.addr) {
				break;
			}
		}
		if(good_iter==goodrelays.end()) {
			goodrelays.push_back(*list_iter);
			gain_bw += list_iter->bandwidth;
			if(gain_bw >= rq.bandwidth && goodrelays.size()>=rq.relaynum)		
				return true;
		}		
	}
	return false;			
}


void SimpleMpathManager::release_resources(int sk) {
	relaylist.clear();
	goodrelays.clear();
}
