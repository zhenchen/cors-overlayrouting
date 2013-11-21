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
#include <sstream>
#include <fstream>
//#include <stdexcept>
#include <stdlib.h>   // rand()
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "advancerldvr.h"
#include "simplenbmtr.h"
#include "neighbor.h"
#include "../utils/debug.h"
#include "../utils/utilfunc.h"

using namespace std;
using namespace cors;

////////////////////////////////////////////////////////
// class AdvanceRldvr
//
AdvanceRldvr::AdvanceRldvr(const vector<string> &param)
  : db_addr(0), db_port(0)
{
  initialize(param);
}

bool AdvanceRldvr::initialize(const vector<string> &param)
{
  vector<string>::size_type n = param.size();
  if (n <= DATABASE_ADDR)
    return false;
  istringstream iss1(param[DATABASE_ADDR]);
  string db_addr_str;
  iss1 >> db_addr_str;
  db_addr = inet_addr(db_addr_str.c_str());

  if (n <= DATABASE_PORT)
    return false;
  istringstream iss2(param[DATABASE_PORT]);
  iss2 >> db_port;
  db_port = htons(db_port);

  in_addr temp;
  temp.s_addr = db_addr;
  DEBUG_PRINT(3, "db_addr = %s\n", inet_ntoa(temp));
  DEBUG_PRINT(3, "db_port = %d\n", ntohs(db_port));

  return true;
}

void AdvanceRldvr::start(int sk, NeighborMaintainer * nbmtr) 
{
  EventQueue * q = EventQueue::Instance();
  q->add_event(new UpdateEvent(time(NULL) + get_update_interval(),
                               this, nbmtr, sk));
}

void AdvanceRldvr::process_rl_inquiry(
    char *msg, int len, sockaddr_in & peeraddr,
    int sk, NeighborMaintainer * nbmtr)
{
  assert(nbmtr != NULL);
#if (defined(RLDVR_INTEST))
  DEBUG_PRINT(4, "AdvanceRldvr::process_rl_inquiry(): (from %s)\n<--: %s\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif

  if (nbmtr->get_neighbor_num() == 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_inquiry(): I have no neighbor\n");
    return;
  }
  DEBUG_PRINT(3, "neighbors = %d\n", nbmtr->get_neighbor_num());

  CorsInquiry inquiry;
  if (inquiry.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_inquiry(): Deserialize() error\n");
    return;
  }
  if (inquiry.required_relays_ == 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_inquiry(): inquiry requires 0 relays\n");
    return;
  }
  inquiry.ttl_--;

  AsnPair key(inquiry.source_.asn, inquiry.destination_.asn);
  AsnHashMap::iterator cache_it = asn_cache.find(key);
  if (cache_it != asn_cache.end()) {
    CorsAdvice advice;
    advice.source_ = inquiry.source_;
    advice.destination_ = inquiry.destination_;
    NList *p_nl = nbmtr->get_neighbor_list();
    NList::reverse_iterator nl_rit = p_nl->rbegin();
    for (int i = 0; nl_rit != p_nl->rend(); ++nl_rit) {
      if (cache_it->second.asn_set.find((*nl_rit)->asn)
          != cache_it->second.asn_set.end()) {
        EndPoint relay;
        relay.asn = (*nl_rit)->asn;
        relay.port = (*nl_rit)->port;
        relay.addr = (*nl_rit)->addr;
        relay.mask = (*nl_rit)->mask;
        advice.relays_.push_back(relay);
        if (++i >= inquiry.required_relays_)
          break;
      }
    }
    if (advice.relays_.size() >= 0) {
      sockaddr_in client;
      client.sin_family = AF_INET;
      client.sin_port = inquiry.source_.port;
      client.sin_addr.s_addr = inquiry.source_.addr;

      int advice_len = 0;
      uint8_t *advice_buf = advice.Serialize(advice_len);
      sendto(sk, advice_buf, advice_len, 0,
             (sockaddr *)&client, sizeof(client));
#if (defined(RLDVR_INTEST))
      DEBUG_PRINT(4, "AdvanceRldvr::process_rl_inquiry(): send advice to client (%s)\n-->: %s\n%s\n", 
                  sockaddr2str(client).c_str(),
                  bytes2str(advice_buf, advice_len).c_str(),
                  advice.ParseToString().c_str());
#endif
      delete[] advice_buf;
    } else {
      Task task(inquiry);
      task_queue.push_back(task);

      asn_cache.erase(cache_it);
    }

    if (advice.relays_.size() < inquiry.required_relays_ &&
        inquiry.ttl_ > 0) {
      // forward inquiry to one of my neighbors
      Neighbor *last_neighbor =
        nbmtr->get_neighbor_list()->back();

      sockaddr_in proxy;
      proxy.sin_family = AF_INET;
      proxy.sin_port = last_neighbor->port;
      proxy.sin_addr.s_addr = last_neighbor->addr;

      CorsInquiry inquiry2(inquiry);
      inquiry2.required_relays_ -= advice.relays_.size();
      int inquiry2_len = 0;
      uint8_t *inquiry2_buf = inquiry2.Serialize(inquiry2_len);
      sendto(sk, inquiry2_buf, inquiry2_len, 0,
             (sockaddr *)&proxy, sizeof(proxy));
#if (defined(RLDVR_INTEST))
      DEBUG_PRINT(4, "AdvanceRldvr::process_rl_inquiry(): forward to neighbor (%s)\n-->: %s\n%s\n", 
                  sockaddr2str(proxy).c_str(),
                  bytes2str(inquiry2_buf, inquiry2_len).c_str(),
                  inquiry2.ParseToString().c_str());
#endif
      delete[] inquiry2_buf;
    }
  } else {
    // didn't find knowledge in cache
    // push inquiry into task_queue 
    // and forward inquiry to database
    Task task(inquiry);
    task_queue.push_back(task);

    sockaddr_in database;
    database.sin_family = AF_INET;
    database.sin_port = db_port;
    database.sin_addr.s_addr = db_addr;

    sendto(sk, msg, len, 0, (sockaddr *)&database, sizeof(database));
#if (defined(RLDVR_INTEST))
    DEBUG_PRINT(4, "AdvanceRldvr::process_rl_inquiry(): ask database (%s)\n-->: %s\n",
              sockaddr2str(database).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif
  }
  return;
}

void AdvanceRldvr::process_rl_report(
    char *msg, int len, sockaddr_in & peeraddr, int sk)
{
  // Simply redirect the report packet to database server
  sockaddr_in database;
  database.sin_family = AF_INET;
  database.sin_port = db_port;
  database.sin_addr.s_addr = db_addr;

  sendto(sk, msg, len, 0, (sockaddr *) & database, sizeof(sockaddr));
#if (defined(RLDVR_INTEST))
  DEBUG_PRINT(4, "AdvanceRldvr::process_rl_report(): from (%s) report to database (%s)\n-->: %s\n",
              sockaddr2str(peeraddr).c_str(),
              sockaddr2str(database).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif
}

void AdvanceRldvr::process_rl_filter(
    char *msg, int len, sockaddr_in & peeraddr, 
    int sk, NeighborMaintainer * nbmtr)
{
  assert(nbmtr != NULL);
#if (defined(RLDVR_INTEST))
  DEBUG_PRINT(4, "AdvanceRldvr::process_rl_filter(): (from %s)\n<--: %s\n",
              sockaddr2str(peeraddr).c_str(),
              bytes2str((unsigned char *)msg, len).c_str());
#endif

  if (nbmtr->get_neighbor_num() == 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_filter(): I have no neighbor\n");
    return;
  }
  DEBUG_PRINT(3, "neighbors = %d\n", nbmtr->get_neighbor_num());
 
  CorsAdvice query_result;
  if (query_result.Deserialize((uint8_t *)msg, len) < 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_filter(): Deserialize() error\n");
    return;
  }
  if (query_result.relays_.size() == 0) {
    DEBUG_PRINT(1, "AdvanceRldvr::process_rl_filter(): result contains 0 relay\n");
    return;
  }
  
  Entry entry;
  entry.timestamp = time(NULL);
  list<EndPoint>::iterator ep_it = query_result.relays_.begin();
  for (; ep_it != query_result.relays_.end(); ++ep_it) {
    entry.asn_set.insert(ep_it->asn);
  }

  CorsAdvice advice;
  NList *p_nl = nbmtr->get_neighbor_list();
  NList::reverse_iterator nl_rit = p_nl->rbegin();
  for (int i = 0; nl_rit != p_nl->rend(); ++nl_rit) {
    if (entry.asn_set.find((*nl_rit)->asn) != entry.asn_set.end()) {
      EndPoint relay;
      relay.asn = (*nl_rit)->asn;
      relay.port = (*nl_rit)->port;
      relay.addr = (*nl_rit)->addr;
      relay.mask = (*nl_rit)->mask;
      advice.relays_.push_back(relay);
      if (++i >= MAX_ADVICED_RELAYS)
        break;
    }
  }
  if (advice.relays_.size() == 0) {
    // incoming knowledge match none of my neighbors
    // ignore this knowledge
    DEBUG_PRINT(3, "AdvanceRldvr::process_rl_filter(): result matches none of my neighbors\n");
    return;
  }

  AsnPair key(query_result.source_.asn, query_result.destination_.asn);
  asn_cache[key] = entry;

  list<Task>::iterator task_it = task_queue.begin();
  for (; task_it != task_queue.end();) {
    if (task_it->inquiry_.source_.asn != key.src_asn ||
        task_it->inquiry_.destination_.asn != key.dst_asn) {
      ++task_it;
    } else {
      // response to this inquiry and remove it from task_queue
      advice.source_ = task_it->inquiry_.source_;
      advice.destination_ = task_it->inquiry_.destination_;
      
      sockaddr_in client;
      client.sin_family = AF_INET;
      client.sin_port = task_it->inquiry_.source_.port;
      client.sin_addr.s_addr = task_it->inquiry_.source_.addr;

      int advice_len = 0;
      uint8_t *advice_buf = advice.Serialize(advice_len);
      sendto(sk, advice_buf, advice_len, 0,
             (sockaddr *)&client, sizeof(client));
#if (defined(RLDVR_INTEST))
      DEBUG_PRINT(4, "AdvanceRldvr::process_rl_filter(): send advice to client (%s)\n-->: %s\n%s\n", 
                  sockaddr2str(client).c_str(),
                  bytes2str(advice_buf, advice_len).c_str(),
                  advice.ParseToString().c_str());
#endif
      delete[] advice_buf;
      if (advice.relays_.size() < task_it->inquiry_.required_relays_ &&
          task_it->inquiry_.ttl_ > 0) {
        // forward inquiry to one of my neighbors
        Neighbor *last_neighbor =
          nbmtr->get_neighbor_list()->back();

        sockaddr_in proxy;
        proxy.sin_family = AF_INET;
        proxy.sin_port = last_neighbor->port;
        proxy.sin_addr.s_addr = last_neighbor->addr;

        CorsInquiry inquiry2(task_it->inquiry_);
        inquiry2.required_relays_ -= advice.relays_.size();
        int inquiry2_len = 0;
        uint8_t *inquiry2_buf = inquiry2.Serialize(inquiry2_len);
        sendto(sk, inquiry2_buf, inquiry2_len, 0,
               (sockaddr *)&proxy, sizeof(proxy));
#if (defined(RLDVR_INTEST))
        DEBUG_PRINT(4, "AdvanceRldvr::process_rl_filter(): forward to neighbor (%s)\n-->: %s\n%s\n", 
                    sockaddr2str(proxy).c_str(),
                    bytes2str(inquiry2_buf, inquiry2_len).c_str(),
                    inquiry2.ParseToString().c_str());
#endif
        delete[] inquiry2_buf;
      }
      // remove this task
      task_it = task_queue.erase(task_it);
    }
  }
  return;
}

void AdvanceRldvr::update(int sk, NeighborMaintainer * nbmtr) 
{
  srand(time(NULL));
  while (task_queue.size() > 0 &&
         task_queue.front().timestamp_.tv_sec < time(NULL)) {
    // response inquiry with a random-selected advice
    if (nbmtr->get_neighbor_num() == 0) {
      task_queue.pop_front();
      continue;
    }
    CorsInquiry inquiry(task_queue.front().inquiry_);
    double ratio = 2 * (double)inquiry.required_relays_ / nbmtr->get_neighbor_num();

    CorsAdvice advice;
    NList *p_nl = nbmtr->get_neighbor_list();
    NList::reverse_iterator nl_rit = p_nl->rbegin();
    for (int i = 0; nl_rit != p_nl->rend(); ++nl_rit) {
      double rnd = (double)rand() / RAND_MAX;
      if (rnd <= ratio) { 
        EndPoint relay;
        relay.asn = (*nl_rit)->asn;
        relay.port = (*nl_rit)->port;
        relay.addr = (*nl_rit)->addr;
        relay.mask = (*nl_rit)->mask;
        advice.relays_.push_back(relay);
        if (++i >= MAX_ADVICED_RELAYS)
          break;
      }
    }
    if (advice.relays_.size() > 0) {
      advice.source_ = inquiry.source_;
      advice.destination_ = inquiry.destination_;

      sockaddr_in client;
      client.sin_family = AF_INET;
      client.sin_port = inquiry.source_.port;
      client.sin_addr.s_addr = inquiry.source_.addr;

      int advice_len = 0;
      uint8_t *advice_buf = advice.Serialize(advice_len);
      sendto(sk, advice_buf, advice_len, 0,
             (sockaddr *)&client, sizeof(client));
#if (defined(RLDVR_INTEST))
      DEBUG_PRINT(4, "AdvanceRldvr::process_rl_inquiry(): send advice to client (%s)\n-->: %s\n%s\n", 
                  sockaddr2str(client).c_str(),
                  bytes2str(advice_buf, advice_len).c_str(),
                  advice.ParseToString().c_str());
#endif
      delete[] advice_buf;
    }
    task_queue.pop_front();
  }

  // clear knowledge older than 120s
  AsnHashMap::iterator cache_it = asn_cache.begin();
  while (cache_it != asn_cache.end()) {
    if (cache_it->second.timestamp + 120 < time(NULL)) {
      asn_cache.erase(cache_it++);
    } else {
      ++cache_it;
    }
  }

  return;
}
