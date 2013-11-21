// Copyright (C) 2007 Hui Zhang <zhang.w.hui@gmail.com>
// //  
// // This program is free software; you can redistribute it and/or modify
// // it under the terms of the GNU General Public License as published by
// // the Free Software Foundation; either version 2 of the License, or
// // (at your option) any later version.
// //  
// // This program is distributed in the hope that it will be useful,
// // but WITHOUT ANY WARRANTY; without even the implied warranty of
// // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// // GNU General Public License for more details.
// //  
// // You should have received a copy of the GNU General Public License
// // along with this program; if not, write to the Free Software
// // Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// //  


#include "simplefwd.h"
#include "../utils/utilfunc.h"

/* update hash map daemon */
void *update_daemon(void *pparam) {
	SimpleFwd *updatefwd = static_cast<SimpleFwd *>(pparam);
	struct timeval hmap_update_sec;
  
  while (true) {
	  hmap_update_sec.tv_sec = updatefwd->_update_interval;
	  hmap_update_sec.tv_usec = 0;
	  select(FD_SETSIZE, NULL, NULL, NULL, &hmap_update_sec);
	  updatefwd->update_fwd_hmap();
  }
}


SimpleFwd::SimpleFwd(vector<string> & param) 
	: _timeout_sec(20), _update_interval(20),	_max_flow(200),_log_flag(true),_log_file("simplefwd.log")
{
	parse_param(param);
  _cur_rq_bw = 0;
	_fwd_hmap.clear();
	if(_log_flag) {
		_ofs.open(_log_file.c_str() ,ios::app); 
		if (!_ofs) {
			DEBUG_PRINT(1, "Open SimpleFwd Output File Error\n");
			exit(-1); 
		}
		struct timeval cur_time;    
		gettimeofday(&cur_time, NULL);
		log_load(_ofs, cur_time);
	}
 
	pthread_mutex_init(&_fwd_hmap_mutex, NULL);
	int ret = pthread_create(&_update_id, NULL, update_daemon, this);
	if (ret != 0) {	
		DEBUG_PRINT(1, "Creating update thread error.\n");
		exit(-1);
	}	
}


void SimpleFwd::parse_param(vector<string> &param) {
	vector<string>::size_type n = param.size();

	//relay session timeout second
	if (n <= TIMEOUT_SEC) return;
	istringstream iss1(param[TIMEOUT_SEC]);
	iss1 >> _timeout_sec;

	//update hash map interval
	if (n <= UPDATE_INTERVAL) return;
	istringstream iss2(param[UPDATE_INTERVAL]);
	iss2 >> _update_interval;

	//max flows could be relayed 
	if (n <= MAX_FLOW) return;
	istringstream iss5(param[MAX_FLOW]);
	iss5 >> _max_flow;

	//whether log the relay session info or not
	if (n <= LOG_FLAG) return;
	istringstream iss6(param[LOG_FLAG]);
	iss6 >> _log_flag;

	//log file name
	if (n <= LOG_FILE) return;
	_log_file = param[LOG_FILE];
}


SimpleFwd::~SimpleFwd() {
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	bool chg = false;

	pthread_join(_update_id, NULL);
	if (_log_flag) {
		Fwd_HMap::iterator iter;
		for(iter=_fwd_hmap.begin(); iter!=_fwd_hmap.end(); iter++) {
			iter->second.end_time = cur_time;
			log_fwd_record(_ofs, iter);
		}
		if(_fwd_hmap.size()>0) chg = true;
	}

	_cur_rq_bw = 0;
	_fwd_hmap.clear();

	if (_log_flag) {
		if(chg) log_load(_ofs, cur_time);
	}
	_ofs.close();
}


/* check whether the relay session is timeout 
	if yes, erase it from the hash map
*/
void SimpleFwd::update_fwd_hmap() {
	Fwd_HMap::iterator iter, pre_iter;
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	bool chg = false;
    
	pthread_mutex_lock(&_fwd_hmap_mutex);
	if(!_fwd_hmap.empty()) {
		iter = _fwd_hmap.begin();
		while(iter != _fwd_hmap.end()) {
			if((cur_time.tv_sec - iter->second.last_contact.tv_sec) > _timeout_sec) {
				_cur_rq_bw -= iter->second.rq_bw;
				iter->second.end_time = cur_time;
				if(_log_flag){
					log_fwd_record(_ofs, iter);
				}
				_fwd_hmap.erase(iter ++);
				chg = true;
			} else {
				iter ++;
			}
		}
	}
	if (_log_flag) {
		if(chg) log_load(_ofs, cur_time);
	}
	pthread_mutex_unlock(&_fwd_hmap_mutex);
}
     

/* Tell the src its current resource
	1.remain bandwith
	2.relay session num
*/
void SimpleFwd::process_probe(char * msg, int len, sockaddr_in & peeraddr, int sk) {
	msg[Control::TYPE_POS] = TYPE_FW_FORWARD;
	msg[Control::TTL_POS]--;

	pthread_mutex_lock(&_fwd_hmap_mutex);
	uint32_t n_avail_bw = htons(MAX_FWD_BW - _cur_rq_bw);
	bcopy(&n_avail_bw, msg+Control::BW_POS, Control::BW_LEN);
	int session_n = htonl(_fwd_hmap.size());
	bcopy(&session_n, msg+Control::TASK_POS, Control::TASK_LEN);
	pthread_mutex_unlock(&_fwd_hmap_mutex);
    
	bcopy(msg+Control::DST_PORT_POS, &peeraddr.sin_port, 6);
	sendto(sk, msg, len, 0, (struct sockaddr*)&peeraddr, sizeof(struct sockaddr));
}


/*1 insert new key and allocate resource for a new session
	2 or else reallocate the resource for an exist session
*/ 
void SimpleFwd::process_bw_reserve(char * msg, int len, sockaddr_in & peeraddr, int sk) {
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	msg[Control::TYPE_POS] = TYPE_BW_FORWARD;
	msg[Control::TTL_POS]--;

	uint32_t n_rq_bw;
	bcopy(msg+Control::BW_POS, &n_rq_bw, Control::BW_LEN);
	uint32_t rq_bw = ntohl(n_rq_bw);

	session_key key;
	bcopy(msg+Control::SRC_PORT_POS, &(key.src_port), 6);
	bcopy(msg+Control::DST_PORT_POS, &(key.dst_port), 6);
	
	pthread_mutex_lock(&_fwd_hmap_mutex);
	Fwd_HMap::iterator iter = _fwd_hmap.find(key);
	if(iter != _fwd_hmap.end()) {
		if(rq_bw != iter->second.rq_bw) {
			_cur_rq_bw -= iter->second.rq_bw;
			iter->second.rq_bw = (MAX_FWD_BW-_cur_rq_bw)>=rq_bw ? rq_bw : (MAX_FWD_BW-_cur_rq_bw);
			rq_bw = iter->second.rq_bw;
			_cur_rq_bw += rq_bw;
			if(_log_flag){ 
				log_load(_ofs, cur_time);     
			}
		}
		iter->second.last_contact = cur_time;
	} else {
		fwd_records record(cur_time);
		record.rq_bw = (MAX_FWD_BW-_cur_rq_bw)>=rq_bw ? rq_bw : (MAX_FWD_BW-_cur_rq_bw);
		rq_bw = record.rq_bw;
		_cur_rq_bw += rq_bw;
	
		if(_fwd_hmap.size() < _max_flow) {
			_fwd_hmap.insert(pair<session_key, fwd_records>(key, record));
	    
			/*  "#### insert into fwdmap #### key = ";
			for (int i = 0; i < KEY_LEN; i++) {
				cout << hex << (int)((uint8_t*)(&key)+i) << " ";
			}   
			cout << dec << endl; */

			if(_log_flag) {
				log_load(_ofs, cur_time);
			}
		}
	}
	n_rq_bw = htons(rq_bw);
	bcopy(&n_rq_bw, msg+Control::BW_POS, Control::BW_LEN);
	int session_n = htonl(_fwd_hmap.size());
	bcopy(&session_n, msg+Control::TASK_POS, Control::TASK_LEN);
	pthread_mutex_unlock(&_fwd_hmap_mutex);
    
	bcopy(msg+Control::DST_PORT_POS, &peeraddr.sin_port, 6);
	DEBUG_PRINT(4, "send bw_forward to %s\n", sockaddr2str(peeraddr).c_str());
	// cout << "bw_forward packet: " << bytes2str((unsigned char *)msg, len) << endl;
	sendto(sk, msg, len, 0, (struct sockaddr*)&peeraddr, sizeof(struct sockaddr));
}

/* release the resource of a finished session */
void SimpleFwd::process_bw_release(char * msg, int len, sockaddr_in & peeraddr, int sk) {
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	session_key key;
	bcopy(msg+Control::SRC_PORT_POS, &(key.src_port), 6);
	bcopy(msg+Control::DST_PORT_POS, &(key.dst_port), 6);
	
	pthread_mutex_lock(&_fwd_hmap_mutex);
	Fwd_HMap::iterator iter = _fwd_hmap.find(key);
	if(iter != _fwd_hmap.end()) {
		_cur_rq_bw -= iter->second.rq_bw;
		iter->second.end_time = cur_time;
	if(_log_flag) {
		log_fwd_record(_ofs, iter);
		log_load(_ofs, cur_time);
	}
	_fwd_hmap.erase(iter);
	}
	pthread_mutex_unlock(&_fwd_hmap_mutex);
}


/* relay a date packet for a session
  whose key is in the hash tabel
 */
void SimpleFwd::process_data_packet(char * msg, int len, sockaddr_in & peeraddr, int sk) {
	session_key key;
	struct timeval cur_time, data_timeout_sec;
	Fwd_HMap::iterator iter;
	DEBUG_PRINT(4, "recv packet len: %d from %s\n", len, sockaddr2str(peeraddr).c_str());

	if(len >= MIN_DATA_PKT_LEN) {
		gettimeofday(&cur_time, NULL);
		bcopy(&peeraddr.sin_port, &(key.src_port), 6);
		bcopy(msg+DATA_ADDR_OFFSET, &(key.dst_port), 6); 
	  
		/*  for (int i = 0; i < KEY_LEN; i ++) {
			cout << hex << (int)(*((uint8_t*)(&key)+i)) << " ";
		}
		cout << dec << endl; */
	  
		pthread_mutex_lock(&_fwd_hmap_mutex);
		iter = _fwd_hmap.find(key);
		if(iter != _fwd_hmap.end()) {								
			bcopy(&peeraddr.sin_port, msg+DATA_ADDR_OFFSET, 6);
			bcopy(&(key.dst_port), &peeraddr.sin_port, 6);	  												
			iter->second.last_contact = cur_time;
			iter->second.fwd_packet++;
			iter->second.fwd_byte += len;

      uint16_t seq = 0;
      memcpy(&seq, msg+11, sizeof(uint16_t));
      seq = ntohs(seq);

			DEBUG_PRINT(4, "forward packet seq: %d to %s\n", seq, sockaddr2str(peeraddr).c_str());
			// DEBUG_PRINT(6, "pakcet: %s\n", bytes2str((unsigned char*)msg, len).c_str());
			sendto(sk, msg, len, 0, (sockaddr *)&peeraddr, sizeof(peeraddr));
		}
		pthread_mutex_unlock(&_fwd_hmap_mutex);
	}
}


void SimpleFwd::log_fwd_record(ostream &os, Fwd_HMap::iterator iter) {
	os<<"Record: ";
	for(int i=0; i<KEY_LEN; i++) {
		os<< hex << (int)(&(iter->first)+i) << " ";
	}
	os << dec << endl;
	os<<iter->second.start_time.tv_sec<<" "
		<<iter->second.start_time.tv_usec<<" "<<iter->second.end_time.tv_sec
		<<" "<<iter->second.end_time.tv_usec<<" "<<iter->second.rq_bw<<" "
		<<iter->second.fwd_packet<<" "<<iter->second.fwd_byte<<endl;
}


void SimpleFwd::log_load(ostream &os, struct timeval tv) {
	os<<"Load: "<<tv.tv_sec<<" "<<tv.tv_usec<<" "
		<<_fwd_hmap.size()<<" "<<_cur_rq_bw<<endl;
}

