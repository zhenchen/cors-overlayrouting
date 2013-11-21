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
#include "statmpathmanager.h"

// header of the direct data packet
typedef struct Directdataheader{
   unsigned type:8;
   unsigned ver:4;
   unsigned direct:1;
   unsigned control:3;
   unsigned reserve:8;
}Directdataheader;

// header of the indirect date packet
typedef struct Indirectdataheader{
   Directdataheader directheader;
   uint16_t port;
   uint32_t ip;
}Indirectdataheader;

// stat packet header
struct stat_header{
	uint8_t type;
	uint16_t port;
	uint32_t addr;
	uint32_t pkt;
	timeval ts;
};
#define STAT_MSG_LEN sizeof(stat_header)

StatMpathManager::StatMpathManager(CorsSocket * sk, vector<string> &param) : MultipathManager(sk) {
	stat_on = DEFAULT_STAT_ON; 
	stat_off = DEFAULT_STAT_OFF;
	
	vector<string>::size_type n = param.size();
	if (n <= STATON) return;
	istringstream iss1(param[STATON]);
	iss1 >> stat_on;    

	if (n <= STATOFF) return;
	istringstream iss2(param[STATOFF]);
	iss2 >> stat_off;   
	
	pthread_mutex_init(&_map_mutex, NULL);
}


int StatMpathManager::send_packet(char * buff, int nbytes, 
	const sockaddr_in & dst, const Feature & ft, int sk) {
	static int cur_channel = 0;		
	static bool bstat = true;
	static int send_num = 0;
		
	char sbuf[CORS_MAX_MSG_SIZE];
	Directdataheader *Dheader;
	Indirectdataheader *Indheader;
	struct sockaddr_in send_addr;
	memcpy((void*)&send_addr, (void*)(&dst), sizeof(struct sockaddr_in));
		
	if(ft.significance == IMPORTANCE){
		//mesh sending
		Dheader = (Directdataheader*)(sbuf);
		Dheader->type = TYPE_DATA;
		Dheader->ver = VERSION;
		Dheader->direct = bstat==true? FOUR: ZERO;
		Dheader->control = ZERO;
		memcpy(sbuf+sizeof(Directdataheader), buff, nbytes);
		sendto(sk, (void*)sbuf, nbytes+sizeof(Directdataheader), 0, 
			(struct sockaddr *)&send_addr, sizeof(struct sockaddr));	
		DEBUG_PRINT(4, "Direct mesh sending!\n");
		
		Indheader=(Indirectdataheader*)(sbuf);
		Indheader->directheader.type = TYPE_DATA;
		Indheader->directheader.ver = VERSION;
		Indheader->directheader.direct= bstat==true? FOUR: ZERO;
		Indheader->directheader.control = ZERO;
		Indheader->ip = dst.sin_addr.s_addr;
		Indheader->port = dst.sin_port;	
		memcpy(sbuf+sizeof(Indirectdataheader), buff, nbytes);
		
		for(int i=0; i<goodrelays.size();i++) {
			send_addr.sin_addr.s_addr = goodrelays.at(i).relay.addr;
			send_addr.sin_port = goodrelays.at(i).relay.port;
			sendto(sk, sbuf, nbytes+sizeof(Indirectdataheader), 0, 
				(struct sockaddr *)&send_addr, sizeof(struct sockaddr)); 
			
			pthread_mutex_lock(&_map_mutex);
			// add send packet num
			if(bstat)snd_map[send_addr.sin_addr.s_addr]++;
			pthread_mutex_unlock(&_map_mutex);
			DEBUG_PRINT(4, "Inirect mesh sending!\n");
		}
	} else if (ft.significance == NORMAL){
		//round robin sendig
		cur_channel = (cur_channel+1)%(goodrelays.size()+1);
		if(cur_channel == goodrelays.size()) {
			Dheader = (Directdataheader*)(sbuf);
			Dheader->type = TYPE_DATA;
			Dheader->ver = VERSION;
			Dheader->direct = bstat==true? FOUR: ZERO;
			Dheader->control = ZERO;
			memcpy(sbuf+sizeof(Directdataheader), buff, nbytes);
			sendto(sk, (void*)sbuf, nbytes+sizeof(Directdataheader), 0, 
				(struct sockaddr *)&send_addr, sizeof(struct sockaddr));
			DEBUG_PRINT(4, "Direct round robin sendig!\n");
		}	else {
			Indheader = (Indirectdataheader*)(sbuf);
			Indheader->directheader.type = TYPE_DATA;
			Indheader->directheader.ver = VERSION;
			Indheader->directheader.direct = bstat==true? FOUR: ZERO;
			Indheader->directheader.control = ZERO;
			Indheader->ip = dst.sin_addr.s_addr;
			Indheader->port = dst.sin_port;	

			send_addr.sin_addr.s_addr = goodrelays.at(cur_channel).relay.addr;
			send_addr.sin_port = goodrelays.at(cur_channel).relay.port;
			sendto(sk, sbuf, nbytes+sizeof(Indirectdataheader), 0, 
					(struct sockaddr *)&send_addr, sizeof(struct sockaddr));
			
			pthread_mutex_lock(&_map_mutex);		
			// add send packet num
			if(bstat)snd_map[send_addr.sin_addr.s_addr]++;
			pthread_mutex_unlock(&_map_mutex);
			DEBUG_PRINT(4, "Inirect round robin sendig!\n");
		}
	}
				
	send_num++;
	if(bstat==true && send_num>=stat_on) {
		//stop stat
		bstat = false;
		send_num = 0;
	}
	else if(bstat==false && send_num>=stat_off) {
		//start stat
		map<uint32_t, uint32_t>::iterator map_iter;
		pthread_mutex_lock(&_map_mutex);	
		for(map_iter=snd_map.begin(); map_iter!=snd_map.end(); map_iter++)
			map_iter->second=0;
		pthread_mutex_unlock(&_map_mutex);	
		bstat = true;
		send_num = 0;	
	}
	return nbytes;
}


char *
StatMpathManager::parse_data_packet(char * buff, int & bufsize, 
	in_addr_t & saddr, in_port_t & sport, sockaddr_in &peeraddr, int sk,
  int * flag) {
	struct sockaddr_in from_in;
	Indirectdataheader	*Indheader;
	Indheader=(Indirectdataheader *)(buff);
	if(Indheader->directheader.ver > VERSION) {
		DEBUG_PRINT(3, "Please update your CORS API!\n");
		return NULL;
	}
	else if(Indheader->directheader.direct == TRUE) {
		DEBUG_PRINT(4, "Direct receive!\n");
		buff = buff + sizeof(Directdataheader);
		bufsize = bufsize - sizeof(Directdataheader);
		sport = peeraddr.sin_port;
		saddr = peeraddr.sin_addr.s_addr;
    if (flag != NULL)
      *flag = 0;
		return buff;
	} else {
		DEBUG_PRINT(4, "Indirect receive!\n");
		memcpy(&sport, buff + PORT_OFFSET, sizeof(sport));
    memcpy(&saddr, buff + ADDR_OFFSET, sizeof(saddr));
    buff = buff + sizeof(Indirectdataheader) ;
		bufsize = bufsize - sizeof(Indirectdataheader);
		
		// add receive packet num
		map<uint32_t, uint32_t>::iterator map_iter;
		in_addr_t relay_addr = peeraddr.sin_addr.s_addr;
		map_iter = rev_map.find(relay_addr);
		if(Indheader->directheader.control >= FOUR) {
			if(map_iter != rev_map.end())map_iter->second++;
			else rev_map.insert(make_pair(relay_addr,1)); 
		} else {
    	if(map_iter != rev_map.end() && map_iter->second >0) {
    		struct sockaddr_in addr;
 				addr.sin_port = sport;
 				addr.sin_addr.s_addr = saddr;			
    		char sbuf[STAT_MSG_LEN];
    		stat_header * header=(stat_header*)(sbuf);
    		header->type = TYPE_STAT;
    		memcpy((void*)(&(header->port)), (void*)(&peeraddr.sin_port),6);

		   long ltmp = htonl(map_iter->second);
    		memcpy((void*)(&(header->pkt)), (void*)&ltmp,4);

    		int pkt_num=htonl(map_iter->second);
    		memcpy((void*)(&(header->pkt)), (void*)(&pkt_num),4);

    		struct timeval cur_time;
    		gettimeofday(&cur_time, NULL);
    		memcpy((void*)(&(header->ts)), (void*)(&cur_time),sizeof(timeval));
    		
    		sendto(sk, (void*)sbuf, STAT_MSG_LEN, 0, 
					(struct sockaddr *)&addr, sizeof(struct sockaddr));	
    		map_iter->second = 0;
    		DEBUG_PRINT(4, "Send stat!!\n");
    	}
  	}
    if (flag != NULL)
      *flag = 1;
  	return buff; 		
	}
}
    

void StatMpathManager::process_path_statistics(char * msg, int nbytes, 
	sockaddr_in & peer, int sk) {
 	DEBUG_PRINT(4, "Rev stat!!\n");
 	uint32_t relay_addr;
 	uint16_t rev_pkt;
 	stat_header * header=(stat_header*)(msg);
 	memcpy((void*)(&relay_addr), (void*)(&header->addr), 4);
  memcpy((void*)(&rev_pkt), (void*)(&header->pkt), 4);	
  rev_pkt = ntohl(rev_pkt);

	map<uint32_t, uint32_t>::iterator map_iter;
	pthread_mutex_lock(&_map_mutex);
	map_iter = snd_map.find(relay_addr);
	// record the stat packet loss rate!!
	if(map_iter != snd_map.end() && map_iter->second >0) {
		for(int i=0; i<goodrelays.size();i++) {
			if(relay_addr == goodrelays.at(i).relay.addr) {
				if (goodrelays.at(i).lossrate == 0)
					goodrelays.at(i).lossrate = 100-rev_pkt*100/map_iter->second;
				else {
					goodrelays.at(i).lossrate += 100-rev_pkt*100/map_iter->second;
					goodrelays.at(i).lossrate /= 2;
				}
				break;
		  }
		}
		map_iter->second = 0;
	}
	pthread_mutex_unlock(&_map_mutex);
}

  
bool StatMpathManager::get_requirement(Requirement & rq) {
	int gain_bw=0;
	vector<RelayNode>::iterator good_iter,bad_iter;
	list<RelayNode>::iterator list_iter;
	relaylist.clear();
	(psk->get_rlysltr())->get_relaylist(relaylist);
	
	pthread_mutex_lock(&_map_mutex);	
	if(relaylist.empty())	{
		pthread_mutex_unlock(&_map_mutex);
		return false;
	}
	for(good_iter=goodrelays.begin(); good_iter!=goodrelays.end();) {
		for(list_iter=relaylist.begin(); list_iter!=relaylist.end();list_iter++) {
			if(good_iter->relay.addr==list_iter->relay.addr){
				if(good_iter->lossrate<rq.lossrate) {
					gain_bw += (good_iter)->bandwidth;
					good_iter++;
				}
				else{
					badrelays.push_back(*good_iter);
					goodrelays.erase(good_iter);
				}
				break;
			}
		}
		if(list_iter==relaylist.end()){
			goodrelays.erase(good_iter);
		}
	}
			
	if(gain_bw >= rq.bandwidth && goodrelays.size()>=rq.relaynum){
			pthread_mutex_unlock(&_map_mutex);		
			return true;	
	}
	
	for(list_iter=relaylist.begin(); list_iter!=relaylist.end(); list_iter++) {
		bool bad_flag=false;
		for(bad_iter=badrelays.begin(); bad_iter!=badrelays.end();bad_iter++) {
			if(bad_iter->relay.addr==list_iter->relay.addr) {
				bad_flag=true;
				break;
			}
		}
		if(bad_flag)continue;
		
		for(good_iter=goodrelays.begin(); good_iter!=goodrelays.end();good_iter++) {
			if(good_iter->relay.addr==list_iter->relay.addr) {
				break;
			}
		}
		if(good_iter==goodrelays.end()) {
			goodrelays.push_back(*list_iter);
			gain_bw += list_iter->bandwidth;
			if(gain_bw >= rq.bandwidth && goodrelays.size()>=rq.relaynum){
				pthread_mutex_unlock(&_map_mutex);		
				return true;
			}
		}		
	}
	pthread_mutex_unlock(&_map_mutex);	
	return false;	
}


void StatMpathManager::release_resources(int sk) {
	relaylist.clear();
	goodrelays.clear();
	badrelays.clear();
	
	snd_map.clear();
	rev_map.clear();
}
