// Copyright (C) 2007 Zhen Chen <zhenchen3419@gmail.com>
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


#ifndef __CORSDB_TRANSLATOR_H
#define __CORSDB_TRANSLATOR_H

#include <assert.h>
#include <ext/hash_map>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <vector>
#include <set>
#include <map>
#include <ext/hash_map>
#include <iostream>
#include <fstream>
#include <sstream>


#include "../common/corsprotocol.h"
#include "../utils/debug.h"
#include "../utils/utilfunc.h"
#include "../proxy/relayunit.h"

#include "cors_mysql.h"

#define EXPECT_BUF_SIZE  400000
#define LISTEN_PORT  6842  //3421X2    


using namespace __gnu_cxx;
using namespace std;
using namespace cors;
using namespace cors_mysql;


class Translator {
  friend void* dispatcher(void * param);

public:
  Translator();
  ~Translator();

  int  start(void *);
  int  stop(void *);  

private:
  int process_inquiry(char* msg, int len, int sock, sockaddr* saddr);
  int process_report(char* msg, int len);

  in_addr_t db_addr;
  in_port_t db_port;
  
  //    std::string conf_file;
  char buff[cors::CORS_MAX_MSG_SIZE];
  char msg[CORS_MAX_MSG_SIZE];

private:
  void parse_param(vector<string> &param);
  void log_append(ostream &os, struct timeval tv);

  time_t _timeout_sec;
  time_t _interval;

  uint16_t _data_port;
  struct sockaddr_in _data_addr;
  int _data_sock;

  bool running;
  pthread_t _heartbeat_id;
  pthread_mutex_t _running_mutex;    
  
  ofstream _ofs;

  bool _log_flag;
  string _log_file;

private:
  cors_mysql::DataBase cors;    // MySQL access interface cors
  cors_mysql::RecordSet * rs;  // MySQL access Result
  
};
 
#endif //__CORSDB_TRANSLATOR_H


