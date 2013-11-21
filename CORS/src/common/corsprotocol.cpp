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

#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "corsprotocol.h"
#include "../utils/debug.h"
#include "../utils/utilfunc.h"

using namespace std;
using namespace cors;

///////////////////////////////////////////////////////////
// struct EndPoint
//

/** Print EndPoint as string
 * \return a string describes the endpoint
 */
string EndPoint::AsString() {
  stringstream ss;
  ss << ip2str(addr) << ":" << ntohs(port)
     << " " << ip2str(mask) << " " << ntohs(asn) 
     << flush;
  return ss.str();
}

///////////////////////////////////////////////////////////
// class CorsInquiry
//

/** constructor, set type_ and other members
 */
CorsInquiry::CorsInquiry() {
  type_ = TYPE_RLY_INQUIRY;
  ttl_ = MAX_TTL;
  required_relays_ = 0;
  memset(&source_, 0, sizeof(source_));
  memset(&destination_, 0, sizeof(destination_));
  memset(&proxy_, 0, sizeof(proxy_));
}

/** Serialization
 * \param len will be filled with the length of this packet
 * \return a pointer to the buffer of this packet
 */
uint8_t *CorsInquiry::Serialize(int &len) {
  uint8_t tmpbuf[100];

  int offset = 0;
  
  memcpy(tmpbuf + offset, &type_, 1);
  offset += 1;

  memcpy(tmpbuf + offset, &ttl_, sizeof(ttl_));
  offset += sizeof(ttl_);

  memcpy(tmpbuf + offset, &required_relays_, sizeof(required_relays_));
  offset += sizeof(required_relays_);

  memcpy(tmpbuf + offset, &source_, sizeof(source_));
  offset += sizeof(source_);

  memcpy(tmpbuf + offset, &destination_, sizeof(destination_));
  offset += sizeof(destination_);

  memcpy(tmpbuf + offset, &proxy_, sizeof(proxy_));
  offset += sizeof(proxy_);

  len = offset;
  uint8_t *buf = new uint8_t[len];
  memcpy(buf, tmpbuf, len);

  return buf;
}

/** Deserialization
 * \param buf a const pointer to the buffer
 * \param len the length of buffer
 * \return 0 -- success, -1 -- error
 */
int CorsInquiry::Deserialize(const uint8_t *buf, const int len) {
  int offset = 0;
  memcpy(&type_, buf + offset, 1);
  if (type_ != TYPE_RLY_INQUIRY) {
    DEBUG_PRINT(1, "incoming type %d mismatch, expect %d\n",
                type_, TYPE_RLY_INQUIRY);
    return -1;
  }

  offset += 1;
  if (offset >= len) goto err;
  ttl_ = buf[offset];

  offset += sizeof(ttl_);
  if (offset >= len) goto err;
  required_relays_ = buf[offset];

  offset += sizeof(required_relays_);
  if (offset >= len) goto err;
  memcpy(&source_, buf + offset, sizeof(source_));

  offset += sizeof(source_);
  if (offset >= len) goto err;
  memcpy(&destination_, buf + offset, sizeof(destination_));

  offset += sizeof(destination_);
  if (offset >= len) goto err;
  memcpy(&proxy_, buf + offset, sizeof(proxy_));

  return 0;

err:
  DEBUG_PRINT(1, "Incomplete CorsInquiry packet\n");
  return -1;
}

/** Print the inquiry packet as string
 * \return a string describes the packet
 */
string CorsInquiry::ParseToString() {
  char tmpbuf1[24];
  char tmpbuf2[24];
  stringstream ss;

  ss << "type:" << type_ << "\n";
  ss << "ttl:" << (int)ttl_ << "\n";
  ss << "req:" << (int)required_relays_ << "\n";

  ss << "src:" << source_.AsString() << "\n";
  ss << "dst:" << destination_.AsString() << "\n";
  ss << "prx:" << proxy_.AsString() << flush;

#if 0
  inet_ntop(AF_INET, &source_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &source_.mask, tmpbuf2, 24);
  ss << "src:" << ntohs(source_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(source_.port) << "/"
     << tmpbuf2 << "\n";

  inet_ntop(AF_INET, &destination_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &destination_.mask, tmpbuf2, 24);
  ss << "dst:" << ntohs(destination_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(destination_.port) << "/"
     << tmpbuf2 << "\n";
  
  inet_ntop(AF_INET, &proxy_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &proxy_.mask, tmpbuf2, 24);
  ss << "prx:" << ntohs(proxy_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(proxy_.port) << "/"
     << tmpbuf2 << flush;
#endif
  return ss.str();
}
///////////////////////////////////////////////////////////
// class CorsAdvice
//

/** default constructor */
CorsAdvice::CorsAdvice() {
  type_ = TYPE_RLY_ADVICE;
  ttl_ = MAX_TTL;
  memset(&source_, 0, sizeof(source_));
  memset(&destination_, 0, sizeof(destination_));
  relays_.clear();
}

/** destructor, clear relay_ */
CorsAdvice::~CorsAdvice() {
  relays_.clear();
}

/** Serialization
 * \param len will be filled with the length of this packet
 * \return a pointer to the buffer of this packet
 */
uint8_t *CorsAdvice::Serialize(int &len) {
  uint8_t size = (relays_.size() > MAX_ADVICED_RELAYS) ? MAX_ADVICED_RELAYS : relays_.size();

  len = 1 + sizeof(ttl_) + sizeof(size) +
        (size + 2) * sizeof(EndPoint);

  uint8_t *buf = new uint8_t[len];
  memset(buf, 0, len);

  int offset = 0;

  memcpy(buf + offset, &type_, 1);
  offset += 1;

  memcpy(buf + offset, &ttl_, sizeof(ttl_));
  offset += sizeof(ttl_);

  memcpy(buf + offset, &size, sizeof(size));
  offset += sizeof(size);

  memcpy(buf + offset, &source_, sizeof(source_));
  offset += sizeof(source_);

  memcpy(buf + offset, &destination_, sizeof(destination_));
  offset += sizeof(destination_);

  list<EndPoint>::iterator it = relays_.begin();
  for (int i = 0; i < size; i++) {
    memcpy(buf + offset, &(*it), sizeof(EndPoint));
    offset += sizeof(EndPoint);
    ++it;
  }
  return buf;
}

/** Deserialization
 * \param buf a const pointer to the buffer
 * \param len the length of buffer
 * \return 0 -- success, -1 -- error
 */
int CorsAdvice::Deserialize(const uint8_t *buf, const int len) {
  int offset = 0;
  uint8_t size = 0;
  memcpy(&type_, buf + offset, 1);
  if (type_ != TYPE_RLY_ADVICE &&
      type_ != TYPE_RLY_FILTER) {
    DEBUG_PRINT(1, "incoming type %d mismatch, expect %d or %d\n",
                type_, TYPE_RLY_ADVICE, TYPE_RLY_FILTER);
    return -1;
  }

  offset += 1;
  if (offset >= len) goto err;
  ttl_ = buf[offset];

  offset += sizeof(ttl_);
  if (offset >= len) goto err;
  size = buf[offset];

  offset += sizeof(size);
  if (offset >= len) goto err;
  memcpy(&source_, buf + offset, sizeof(source_));

  offset += sizeof(source_);
  if (offset >= len) goto err;
  memcpy(&destination_, buf + offset, sizeof(destination_));

  offset += sizeof(destination_);
  for (int i = 0; i < size; i++) {
    if (offset >= len)
      goto err;
    EndPoint relay;
    memcpy(&relay, buf + offset, sizeof(relay));
    relays_.push_back(relay);
    offset += sizeof(relay);
  }
  return 0;

err:
  DEBUG_PRINT(1, "Incomplete CorsAdvice packet\n");
  return -1;
}

/** Print the inquiry packet as string
 * \return a string describes the packet
 */
string CorsAdvice::ParseToString() {
  char tmpbuf1[24];
  char tmpbuf2[24];
  stringstream ss;

  ss << "type:" << type_ << "\n";
  ss << "ttl:" << (int)ttl_ << "\n";
  ss << "num:" << relays_.size() << "\n";

  ss << "src:" << source_.AsString() << "\n";
  ss << "dst:" << destination_.AsString() << "\n";
  
#if 0  
  inet_ntop(AF_INET, &source_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &source_.mask, tmpbuf2, 24);
  ss << "src:" << ntohs(source_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(source_.port) << "/"
     << tmpbuf2 << "\n";

  inet_ntop(AF_INET, &destination_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &destination_.mask, tmpbuf2, 24);
  ss << "dst:" << ntohs(destination_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(destination_.port) << "/"
     << tmpbuf2 << "\n";
#endif  
  
  list<EndPoint>::iterator it = relays_.begin();
  for (; it != relays_.end(); ++it) {
    ss << "rly:" << it->AsString() << "\n";
#if 0    
    inet_ntop(AF_INET, &(*it).addr, tmpbuf1, 24);
    inet_ntop(AF_INET, &(*it).mask, tmpbuf2, 24);
    ss << "rly:" << ntohs(it->asn) << "/"
       << tmpbuf1 << ":" << ntohs(it->port) << "/"
       << tmpbuf2 << "\n";
#endif
  }
  ss << flush;
  return ss.str();
}
///////////////////////////////////////////////////////////
// class CorsReport
//

/** default constructor */
CorsReport::CorsReport() {
  type_ = TYPE_RLY_REPORT;
  ttl_ = MAX_TTL;
  memset(&source_, 0, sizeof(source_));
  memset(&destination_, 0, sizeof(destination_));
  reports_.clear();
}

/** destructor, clear relay_ */
CorsReport::~CorsReport() {
  reports_.clear();
}

/** Serialization
 * \param len will be filled with the length of this packet
 * \return a pointer to the buffer of this packet
 */
uint8_t *CorsReport::Serialize(int &len) {
  uint8_t size = (reports_.size() > MAX_REPORT_ENTRYS) ? MAX_REPORT_ENTRYS : reports_.size();

  len = 1 + sizeof(ttl_) + sizeof(size) +
        2 * sizeof(EndPoint) + size * sizeof(ReportEntry);

  uint8_t *buf = new uint8_t[len];
  memset(buf, 0, len);

  int offset = 0;

  memcpy(buf + offset, &type_, 1);
  offset += 1;

  memcpy(buf + offset, &ttl_, sizeof(ttl_));
  offset += sizeof(ttl_);

  memcpy(buf + offset, &size, sizeof(size));
  offset += sizeof(size);

  memcpy(buf + offset, &source_, sizeof(source_));
  offset += sizeof(source_);

  memcpy(buf + offset, &destination_, sizeof(destination_));
  offset += sizeof(destination_);

  list<ReportEntry>::iterator it = reports_.begin();
  for (int i = 0; i < size; i++) {
    memcpy(buf + offset, &(*it), sizeof(ReportEntry));
    offset += sizeof(ReportEntry);
    ++it;
  }
  return buf;
}

/** Deserialization
 * \param buf a const pointer to the buffer
 * \param len the length of buffer
 * \return 0 -- success, -1 -- error
 */
int CorsReport::Deserialize(const uint8_t *buf, const int len) {
  int offset = 0;
  uint8_t size = 0;
  memcpy(&type_, buf + offset, 1);
  if (type_ != TYPE_RLY_REPORT) {
    DEBUG_PRINT(1, "incoming type %d mismatch, expect %d\n",
                type_, TYPE_RLY_REPORT);
    return -1;
  }

  offset += 1;
  if (offset >= len) goto err;
  ttl_ = buf[offset];

  offset += sizeof(ttl_);
  if (offset >= len) goto err;
  size = buf[offset];

  offset += sizeof(size);
  if (offset >= len) goto err;
  memcpy(&source_, buf + offset, sizeof(source_));

  offset += sizeof(source_);
  if (offset >= len) goto err;
  memcpy(&destination_, buf + offset, sizeof(destination_));

  offset += sizeof(destination_);
  for (int i = 0; i < size; i++) {
    if (offset >= len)
      goto err;
    ReportEntry entry;
    memcpy(&entry, buf + offset, sizeof(entry));
    reports_.push_back(entry);
    offset += sizeof(entry);
  }
  return 0;

err:
  DEBUG_PRINT(1, "Incomplete CorsReport packet\n");
  return -1;
}

/** Print the inquiry packet as string
 * \return a string describes the packet
 */
string CorsReport::ParseToString() {
  char tmpbuf1[24];
  char tmpbuf2[24];
  stringstream ss;

  ss << "type:" << type_ << "\n";
  ss << "ttl:" << (int)ttl_ << "\n";
  ss << "num:" << reports_.size() << "\n";

  ss << "src:" << source_.AsString() << "\n";
  ss << "dst:" << destination_.AsString() << "\n";
#if 0  
  inet_ntop(AF_INET, &source_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &source_.mask, tmpbuf2, 24);
  ss << "src:" << ntohs(source_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(source_.port) << "/"
     << tmpbuf2 << "\n";

  inet_ntop(AF_INET, &destination_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &destination_.mask, tmpbuf2, 24);
  ss << "dst:" << ntohs(destination_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(destination_.port) << "/"
     << tmpbuf2 << "\n";
#endif  
  
  list<ReportEntry>::iterator it = reports_.begin();
  for (; it != reports_.end(); ++it) {
    ss << "rly:" << it->relay.AsString() << " "
       << "delay:" << it->delay.tv_sec << ":" << it->delay.tv_usec << "\n";
#if 0    
    inet_ntop(AF_INET, &it->relay.addr, tmpbuf1, 24);
    inet_ntop(AF_INET, &it->relay.mask, tmpbuf2, 24);
    ss << "rly:" << ntohs(it->relay.asn) << "/"
       << tmpbuf1 << ":" << ntohs(it->relay.port) << "/"
       << tmpbuf2 << " " 
       << "delay:" << it->delay.tv_sec << ":" << it->delay.tv_usec << "\n";
#endif
  }
  ss << flush;
  return ss.str();
}
///////////////////////////////////////////////////////////
// class CorsProbe
//

/** default constructor */
CorsProbe::CorsProbe() {
  type_ = TYPE_FW_PROBE;
  ttl_ = MAX_TTL;
  memset(&source_, 0, sizeof(source_));
  memset(&relay_, 0, sizeof(relay_));
  memset(&destination_, 0, sizeof(destination_));
  memset(&timestamp_, 0, sizeof(timestamp_));
  bandwidth_ = 0;
  load_num_ = 0;
}

/** Serialization
 * \param len will be filled with the length of this packet
 * \return a pointer to the buffer of this packet
 */
uint8_t *CorsProbe::Serialize(int &len) {
  len = 1 + sizeof(ttl_) + 3 * sizeof(EndPoint) +
        sizeof(timestamp_) + sizeof(bandwidth_) +
        sizeof(load_num_);

  uint8_t *buf = new uint8_t[len];
  memset(buf, 0, len);

  int offset = 0;

  memcpy(buf + offset, &type_, 1);
  offset += 1;

  memcpy(buf + offset, &ttl_, sizeof(ttl_));
  offset += sizeof(ttl_);

  memcpy(buf + offset, &source_, sizeof(source_));
  offset += sizeof(source_);

  memcpy(buf + offset, &relay_, sizeof(relay_));
  offset += sizeof(relay_);

  memcpy(buf + offset, &destination_, sizeof(destination_));
  offset += sizeof(destination_);

  memcpy(buf + offset, &timestamp_, sizeof(timestamp_));
  offset += sizeof(timestamp_);

  uint32_t b = htonl(bandwidth_);
  memcpy(buf + offset, &b, sizeof(bandwidth_));
  offset += sizeof(bandwidth_);

  uint32_t l = htonl(load_num_);
  memcpy(buf + offset, &l, sizeof(load_num_));
  return buf;
}

/** Deserialization
 * \param buf a const pointer to the buffer
 * \param len the length of buffer
 * \return 0 -- success, -1 -- error
 */
int CorsProbe::Deserialize(const uint8_t *buf, const int len) {
  int offset = 0;
  memcpy(&type_, buf + offset, 1);
  if (type_ != TYPE_FW_PROBE && 
      type_ != TYPE_FW_FORWARD && 
      type_ != TYPE_FW_REPLY && 
      type_ != TYPE_BW_RESERVE &&
      type_ != TYPE_BW_FORWARD &&
      type_ != TYPE_BW_REPLY &&
      type_ != TYPE_BW_RELEASE) {
    return -1;
  }

  offset += 1;
  if (offset >= len) goto err;
  ttl_ = buf[offset];

  offset += sizeof(ttl_);
  if (offset >= len) goto err;
  memcpy(&source_, buf + offset, sizeof(source_));

  offset += sizeof(source_);
  if (offset >= len) goto err;
  memcpy(&relay_, buf + offset, sizeof(relay_));

  offset += sizeof(relay_);
  if (offset >= len) goto err;
  memcpy(&destination_, buf + offset, sizeof(destination_));

  offset += sizeof(destination_);
  if (offset >= len) goto err;
  memcpy(&timestamp_, buf + offset, sizeof(timestamp_));

  offset += sizeof(timestamp_);
  if (offset >= len) goto err;
  memcpy(&bandwidth_, buf + offset, sizeof(bandwidth_));
  bandwidth_ = ntohl(bandwidth_);

  offset += sizeof(bandwidth_);
  if (offset >= len) goto err;
  memcpy(&load_num_, buf + offset, sizeof(load_num_));
  load_num_ = ntohl(load_num_);
  return 0;

err:
  DEBUG_PRINT(1, "Incomplete CorsProbe packet\n");
  return -1;
}

/** Print the inquiry packet as string
 * \return a string describes the packet
 */
string CorsProbe::ParseToString() {
  char tmpbuf1[24];
  char tmpbuf2[24];
  stringstream ss;

  ss << "type:" << type_ << "\n";
  ss << "ttl:" << (int)ttl_ << "\n";

  ss << "src:" << source_.AsString() << "\n";
  ss << "dst:" << destination_.AsString() << "\n";
  ss << "rly:" << relay_.AsString() << "\n";
#if 0
  inet_ntop(AF_INET, &source_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &source_.mask, tmpbuf2, 24);
  ss << "src:" << ntohs(source_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(source_.port) << "/"
     << tmpbuf2 << "\n";

  inet_ntop(AF_INET, &destination_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &destination_.mask, tmpbuf2, 24);
  ss << "dst:" << ntohs(destination_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(destination_.port) << "/"
     << tmpbuf2 << "\n";
  
  inet_ntop(AF_INET, &relay_.addr, tmpbuf1, 24);
  inet_ntop(AF_INET, &relay_.mask, tmpbuf2, 24);
  ss << "rly:" << ntohs(relay_.asn) << "/"
     << tmpbuf1 << ":" << ntohs(relay_.port) << "/"
     << tmpbuf2 << "\n";
#endif
  ss << "ts:" << timestamp_.tv_sec << ":" << timestamp_.tv_usec<< "\n";
  ss << "avbw:" << bandwidth_ << "\n";
  ss << "load:" << load_num_ << flush;

  return ss.str();
}
