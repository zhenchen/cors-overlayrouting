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
//  

#include "translator.h"

// To Generate the insert sql values string
//
string ValueString(const EndPoint& point) {
  stringstream ss;
  ss << ntohs(point.asn) << ", '" << ip2str(point.addr) << "', " << "'" << ip2str(point.mask) << "', " << ntohs(point.port) << flush;

  return ss.str();
}


void *
dispatcher(void *param)
{
    cout<<"enter dispatcher!"<<endl;
    Translator *translator = static_cast<Translator *>(param);

    usleep(500000);

    while(true){

      if(translator->running != true) break;

  }

    cout<<" out of thread!"<<endl;
}




Translator::Translator():_data_port(LISTEN_PORT), _log_file("corsdb.log") 
{
   //create socket() 
    int old_buf_size = 0, buf_size;
    socklen_t socklen;
    _data_sock = socket(AF_INET, SOCK_DGRAM, 0);

  //set socket buf_size option
    getsockopt(_data_sock, SOL_SOCKET, SO_SNDBUF, &old_buf_size, &socklen);
    buf_size = old_buf_size > EXPECT_BUF_SIZE ? old_buf_size : EXPECT_BUF_SIZE;
    setsockopt(_data_sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
    setsockopt(_data_sock, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
    
  //bind socket
  _data_addr.sin_family = AF_INET;
    _data_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&_data_addr.sin_zero, 8);
    bool bind_flag = false;
                  
    _data_addr.sin_port = htons(_data_port);  
    if(bind(_data_sock, (struct sockaddr*)&_data_addr, sizeof(struct sockaddr)) == 0) {
      bind_flag = true;
    }
    
    if(bind_flag == false) {
        DEBUG_PRINT(2, "binding socket failed!\n");
        exit(-1);
    }

  //open log file    
    if(_log_flag) {
        _ofs.open(_log_file.c_str() ,ios::app); 
        if (!_ofs) {
            DEBUG_PRINT(2, "Open Log Output File Error\n");
      exit(-1); 
  }
  struct timeval cur_time;    
  gettimeofday(&cur_time, NULL);
        log_append(_ofs, cur_time);
    }

  //create heartbeat thread
  pthread_mutex_init(&_running_mutex, NULL);
  
  running = true;

  cout << "creating thread" << endl;

  int ret = pthread_create(&_heartbeat_id, NULL, dispatcher, this);

  if (ret != 0) {
    DEBUG_PRINT(2, "Creating process inquery and share thread error.\n");
    exit(-1);
  }

  // create connection to MYSQL Database
  // parameters: @localhost, @username, @passwd, @db_name, @flags
  if(-1 == cors.Connect("localhost", "root", "security421", "overlaypath", 0, 0)){
      std::cout<<"Connection to MYSQL failed "<<std::endl;
      exit(-1);
  }
  else {
      std::cout<<"Connection to MYSQL successfully!"<<std::endl;
  }

   //create the result sets
  rs = new cors_mysql::RecordSet(cors.GetMysql());

  //enter the message handling loop!
  int len;
  char msg[CORS_MAX_MSG_SIZE];
  socklen_t addrlen = sizeof(sockaddr_in);
  struct sockaddr_in addr;
  fd_set fset;

  FD_ZERO(&fset);
  FD_SET(_data_sock, &fset);
  bzero(&addr, sizeof(struct sockaddr_in));
  int fds = _data_sock + 1;

  while(true){

    select(fds, &fset, NULL, NULL, NULL);

    if (FD_ISSET(_data_sock, &fset)) {
      int len = recvfrom(_data_sock, msg, sizeof(msg) / sizeof(char), 0, (sockaddr*)&addr, &addrlen);
  
      if (len > 0) {
          switch (msg[0]) {
            case TYPE_RLY_INQUIRY:
              process_inquiry(msg, sizeof(msg), _data_sock, (sockaddr *) &addr); 
              break;

            case TYPE_RLY_REPORT:
              process_report(msg, sizeof(msg));

              break;

            default:
              break;
         }  
       } 
    }  
  }
}

int 
Translator::start(void *) {

  return 0;
}


void 
Translator::parse_param(vector<string> &param) {


}


Translator::~Translator() {

  cout<<"enter ~Translator!"<<endl;
    
  struct timeval cur_time;

  gettimeofday(&cur_time, NULL);

  //close MYSQL connection firstly
  cors.DisConnect();

  if(rs != NULL){  
    delete rs;
  }
    //close pthread
  running = false;

  pthread_join(_heartbeat_id, NULL);
    
  pthread_mutex_destroy(&_running_mutex);

  close(_data_sock);

  //close file descriptor
  _ofs.close();

  cout<<"end of descontructor!"<<endl;

}


int 
Translator::process_inquiry(char *msg, int len, int sock, sockaddr* saddr){

  CorsInquiry inquiry;
  int ret = inquiry.Deserialize((uint8_t *)msg, len);

  if (ret < 0) {
    DEBUG_PRINT(2, "Parsing inquiry error\n");
    return 0;
  }

  uint16_t rl_num = 0;
  in_addr_t src_ip   = inquiry.source_.addr;
  in_addr_t src_mask = inquiry.source_.mask;
  in_addr_t dst_ip   = inquiry.destination_.addr;
  in_addr_t dst_mask = inquiry.destination_.mask;

  time_t current_time=time(NULL);

  //char ssql[500];
  string ssql;
  char src_net[20] = "src_net";
  struct in_addr srcnet;
  srcnet.s_addr = src_ip&src_mask;
  cout<<src_ip <<" "<< inet_ntoa(srcnet)<<endl;

  char dst_net[20] = "dst_net";
  struct in_addr dstnet;
  dstnet.s_addr = dst_ip&dst_mask;
  cout<<dst_ip <<" "<< inet_ntoa(dstnet)<<endl;
  
  //sprintf(ssql, "select * from path where %s=%s and %s=%s",src_net, inet_ntoa(srcnet), dst_net, inet_ntoa(dstnet));
/*  ssql = "select * from path where " + string(src_net) + "=" +"\"" + string(inet_ntoa(srcnet)) +"\"" +  " and " + string(dst_net) + "=" + "\"" + string(inet_ntoa(dstnet)) + "\"" +";";*/
  //printf("%s \n", ssql);
  ssql = "select * from path;";

  printf("%s \n", ssql.c_str());

  rs->ExecuteSQL(ssql.c_str());

  int count = rs->GetRecordCount();
  cout<<"record number is: "<< count <<endl;

  list<EndPoint> relays;
  EndPoint endpoint;
  CorsAdvice advice;
  advice.type_ = TYPE_RLY_FILTER;
  advice.source_ = inquiry.source_;
  advice.destination_ = inquiry.destination_;

  //  struct NetUnit netunit;
  char sbuf[40];
  rs->MoveFirst();/*Move the index to the first position*/

  while(!rs->IsEof()/*Is the index reach the tail?*/) {
    rs->GetCurrentFieldValue("rly_asn", sbuf);

    //    netunit.net_addr = inet_addr(sbuf);
    endpoint.asn = htons(atoi(sbuf));
    endpoint.addr = 0;
    endpoint.mask = 0;
    endpoint.port = 0;
    
    cout << "rly_asn:" << endpoint.asn <<"\n" << endl;

    advice.relays_.push_back(endpoint);

    rs->MoveNext();
  }

  cout << "record number =: " << advice.relays_.size() << endl;
  
  int temp_len;
  uint8_t * buff = advice.Serialize(temp_len);

  time_t t = time(NULL);
  char *ct = ctime(&t);
  ct[strlen(ct) - 1] = '\0';

  cout << "[" << ct << "]:" << "send to "
       << sockaddr2str(*(sockaddr_in *)saddr) << endl;
  cout << "-->: " << bytes2str((unsigned char *)buff, temp_len) << endl;

  sendto(sock, buff, temp_len, 0, (sockaddr *)&saddr, sizeof(sockaddr));

  delete[] buff;

  return 1;
}

int 
Translator::process_report( char* msg, int len){

  CorsReport report;

  int ret =report.Deserialize((uint8_t *)msg, len);

  if (ret < 0) {
    DEBUG_PRINT(2, "Parsing inquiry error\n");
    return 0;
  }

  stringstream values;
  list<ReportEntry>::iterator index;

  for(index = report.reports_.begin(); index != report.reports_.end(); ++index) {
    DEBUG_PRINT(4, "ReportEntry: %s\n", index->relay.AsString().c_str());

    values << ValueString(report.source_) <<", " 
           << ValueString(report.destination_) << ", "
           << ValueString(index->relay) << ", "
           << (index->delay.tv_sec*1e6 + index->delay.tv_usec) << ", "
           << time(NULL);

    string insert_sql;

    insert_sql = "insert into path values(" +  values.str() + ");";

    printf("%s \n", insert_sql.c_str());

    rs->ExecuteSQL(insert_sql.c_str());
  }

}

void 
Translator::log_append(ostream &os, struct timeval tv) {
  os << "Load: " << tv.tv_sec << " " << tv.tv_usec << " "
     << " " << " " << " " << endl;
}

