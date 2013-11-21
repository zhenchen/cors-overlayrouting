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

#ifndef __COMMON_CORSPROTOCOL_H
#define __COMMON_CORSPROTOCOL_H

#include <list>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <sys/time.h>

using namespace std;

/** namespace cors define 
 */
namespace cors {
  const int CORS_MAX_MSG_SIZE = 2048;
  const uint8_t MAX_ADVICED_RELAYS = 20;
  const uint8_t MAX_REPORT_ENTRYS = 20;
  const uint8_t MAX_RL_NUM = 10;
  //const uint8_t MAX_RP_NUM = 10;
  const uint8_t MAX_FILTER_SIZE = 20;
  const uint8_t MAX_TTL = 4;

  struct Knowledge {
    in_addr_t addr;
    in_addr_t mask;
    uint16_t asn;	// AS id number
    uint64_t delay;
    Knowledge() {
      memset(this, 0, sizeof(Knowledge));
    }
    Knowledge(in_addr_t ad, in_addr_t ma,
              uint16_t as, uint64_t dly)
      : addr(ad), mask(ma), asn(as), delay(dly) {}
  };

  typedef list<Knowledge> KnList;

  /** packet type definition */
  enum PACKET_TYPE {
    TYPE_PROBE_PING, 
    TYPE_PROBE_PONG,
    // add by ghli
    TYPE_RLY_INQUIRY, 
    TYPE_RLY_ADVICE,
    TYPE_RLY_FILTER,
    TYPE_RLY_SHARE, 
    TYPE_RLY_REPORT,
    //add four packet_type by hzhang
    TYPE_FW_PROBE, 
    TYPE_FW_FORWARD, 
    TYPE_FW_REPLY,
    TYPE_BW_RESERVE, 
    TYPE_BW_FORWARD, 
    TYPE_BW_REPLY,
    TYPE_BW_RELEASE,
    
    TYPE_DATA,
    TYPE_STAT
  };

  /** information of an overlay node
   */
  struct EndPoint {
    /** AS number, 2 bytes */
    uint16_t asn;
    /** corsproxy port number, 2 bytes */
    in_port_t port;
    /** ip address, 4 bytes */
    in_addr_t addr;
    /** ip mask, 4 bytes */
    in_addr_t mask;

    string AsString();
  };

  /** entry in CorsReport
   */
  struct ReportEntry {
    /** overlay node information */
    EndPoint relay;
    /** delay though the overlay node */
    timeval delay;
  };

  /** The abstract base class for all control packets in CORS
   */
  class CorsPacket {
   public:
    /** default constructor */
    CorsPacket() {};
    /** destructor */
    ~CorsPacket() {};
    
    /** interface of serialization
     * \param len will be filled with the length of this packet
     * \return a pointer to the buffer of this packet
     */
    virtual uint8_t *Serialize(int &len) = 0;
    
    /** interface of deserialization
     * \param buf a const pointer to the buffer
     * \param len the length of buffer
     * \return 0 -- success, -1 -- error
     */
    virtual int Deserialize(const uint8_t *buf, const int len) = 0;

   public:
    /** type of the packet */
    PACKET_TYPE type_;
    /** time to live */
    uint8_t ttl_;
  };

  /** Inquiry packet
   */
  class CorsInquiry : public CorsPacket {
   public:
    CorsInquiry();
    /** destructor, do nothing */
    ~CorsInquiry() {}

    virtual uint8_t *Serialize(int &len);
    virtual int Deserialize(const uint8_t *buf, const int len);
    string ParseToString();

   public:
    /** required number of relays */
    uint8_t required_relays_;
    /** source node information */
    EndPoint source_;
    /** destination node information */
    EndPoint destination_;
    /** proxy node information */
    EndPoint proxy_;
  };

  /** Advice packet
   */
  class CorsAdvice : public CorsPacket {
   public:
    CorsAdvice();
    ~CorsAdvice();

    virtual uint8_t *Serialize(int &len);
    virtual int Deserialize(const uint8_t *buf, const int len);
    string ParseToString();

   public:
    /** source node information */
    EndPoint source_;
    /** destination node information */
    EndPoint destination_;
    /** list of relay node candidates */
    list<EndPoint> relays_;
  };

  /** Report packet
   */
  class CorsReport : public CorsPacket {
   public:
    CorsReport();
    ~CorsReport();

    virtual uint8_t *Serialize(int &len);
    virtual int Deserialize(const uint8_t *buf, const int len);
    string ParseToString();

   public:
    /** source node information */
    EndPoint source_;
    /** destination node information */
    EndPoint destination_;
    /** reports on the performances of overlay nodes */
    list<ReportEntry> reports_;
  };

  /** Probe packet
   */
  class CorsProbe : public CorsPacket {
   public:
    CorsProbe();
    /** destructor, do nothing */
    ~CorsProbe() {}

    virtual uint8_t *Serialize(int &len);
    virtual int Deserialize(const uint8_t *buf, const int len);
    string ParseToString();

   public:
    /** source node information */
    EndPoint source_;
    /** relay node information */
    EndPoint relay_;
    /** destination node information */
    EndPoint destination_;
    /** the sending time of this packet, for RTT calculation */
    timeval timestamp_;
    /** available bandwidth, filled by relay node */
    uint32_t bandwidth_;
    /** number of streams forwarded by the relay node, filled by relay node */
    uint32_t load_num_;
  };

  // define packet format shared between proxy and client
  struct Inquiry {
    static const int TYPE_POS = 0;
    static const int TYPE_LEN = 1;
    static const int TTL_POS = TYPE_POS + TYPE_LEN; // 1
    static const int TTL_LEN = sizeof(uint8_t);
    static const int NUM_POS = TTL_POS + TTL_LEN; // 2
    static const int NUM_LEN = sizeof(uint8_t);
    static const int SRC_PORT_POS = NUM_POS + NUM_LEN; // 3
    static const int SRC_PORT_LEN = sizeof(in_port_t);
    static const int SRC_ADDR_POS = SRC_PORT_POS + SRC_PORT_LEN; // 5
    static const int SRC_ADDR_LEN = sizeof(in_addr_t);
    static const int SRC_MASK_POS = SRC_ADDR_POS + SRC_ADDR_LEN; // 9
    static const int SRC_MASK_LEN = sizeof(in_addr_t);
    static const int DST_ADDR_POS = SRC_MASK_POS + SRC_MASK_LEN; // 13
    static const int DST_ADDR_LEN = sizeof(in_addr_t);
    static const int DST_MASK_POS = DST_ADDR_POS + DST_ADDR_LEN; // 17
    static const int DST_MASK_LEN = sizeof(in_addr_t);
    static const int PRX_PORT_POS = DST_MASK_POS + DST_MASK_LEN; // 21
    static const int PRX_PORT_LEN = sizeof(in_port_t);
    static const int PRX_ADDR_POS = PRX_PORT_POS + PRX_PORT_LEN; // 23
    static const int PRX_ADDR_LEN = sizeof(in_addr_t);
    static const int UPD_POS = PRX_ADDR_POS + PRX_ADDR_LEN; // 27
    static const int UPD_LEN = sizeof(time_t);
    static const int DLY_POS = UPD_POS + UPD_LEN; // 31
    static const int DLY_LEN = sizeof(uint32_t);
    static const int INQUIRY_LEN = DLY_POS + DLY_LEN; // 35
  };

  struct Advice {
    static const int TYPE_POS = 0;
    static const int TYPE_LEN = 1;
    static const int TTL_POS = TYPE_POS + TYPE_LEN; // 1
    static const int TTL_LEN = sizeof(uint8_t);
    static const int NUM_POS = TTL_POS + TTL_LEN; // 2
    static const int NUM_LEN = sizeof(uint8_t);
    static const int SRC_PORT_POS = NUM_POS + NUM_LEN; // 3
    static const int SRC_PORT_LEN = sizeof(in_port_t);
    static const int SRC_ADDR_POS = SRC_PORT_POS + SRC_PORT_LEN; // 5
    static const int SRC_ADDR_LEN = sizeof(in_addr_t);
    static const int SRC_MASK_POS = SRC_ADDR_POS + SRC_ADDR_LEN; // 9
    static const int SRC_MASK_LEN = sizeof(in_addr_t);
    static const int DST_ADDR_POS = SRC_MASK_POS + SRC_MASK_LEN; // 13
    static const int DST_ADDR_LEN = sizeof(in_addr_t);
    static const int DST_MASK_POS = DST_ADDR_POS + DST_ADDR_LEN; // 17
    static const int DST_MASK_LEN = sizeof(in_addr_t);
    static const int RLS_POS = DST_MASK_POS + DST_MASK_LEN; // 21
    // static const int PRX_PORT_POS = DST_MASK_POS + DST_MASK_LEN; // 21
    // static const int PRX_PORT_LEN = sizeof(in_port_t);
    // static const int PRX_ADDR_POS = PRX_PORT_POS + PRX_PORT_LEN; // 23
    // static const int PRX_ADDR_LEN = sizeof(in_addr_t);
  };
	
  struct Filter {
    static const int TYPE_POS = 0;
    static const int TYPE_LEN = 1;
    static const int TTL_POS = TYPE_POS + TYPE_LEN; // 1
    static const int TTL_LEN = sizeof(uint8_t);
    static const int NUM_POS = TTL_POS + TTL_LEN; // 2
    static const int NUM_LEN = sizeof(uint8_t);
    static const int SRC_PORT_POS = NUM_POS + NUM_LEN; // 3
    static const int SRC_PORT_LEN = sizeof(in_port_t);
    static const int SRC_ADDR_POS = SRC_PORT_POS + SRC_PORT_LEN; // 5
    static const int SRC_ADDR_LEN = sizeof(in_addr_t);
    static const int SRC_MASK_POS = SRC_ADDR_POS + SRC_ADDR_LEN; // 9
    static const int SRC_MASK_LEN = sizeof(in_addr_t);
    static const int SRC_ASID_POS = SRC_MASK_POS + SRC_MASK_LEN; // 13
    static const int SRC_ASID_LEN = sizeof(uint16_t);
    static const int DST_ADDR_POS = SRC_ASID_POS + SRC_ASID_LEN; // 15
    static const int DST_ADDR_LEN = sizeof(in_addr_t);
    static const int DST_MASK_POS = DST_ADDR_POS + DST_ADDR_LEN; // 19
    static const int DST_MASK_LEN = sizeof(in_addr_t);
    static const int DST_ASID_POS = DST_MASK_POS + DST_MASK_LEN; // 23
    static const int DST_ASID_LEN = sizeof(uint16_t);
    static const int RPRT_POS = DST_ASID_POS + DST_ASID_LEN; // 25
  };

  struct Report {
    static const int TYPE_POS = 0;
    static const int TYPE_LEN = 1;
    static const int TTL_POS = TYPE_POS + TYPE_LEN; // 1
    static const int TTL_LEN = sizeof(uint8_t);
    static const int NUM_POS = TTL_POS + TTL_LEN; // 2
    static const int NUM_LEN = sizeof(uint8_t);
    static const int SRC_ADDR_POS = NUM_POS + NUM_LEN; // 3
    static const int SRC_ADDR_LEN = sizeof(in_addr_t);
    static const int SRC_MASK_POS = SRC_ADDR_POS + SRC_ADDR_LEN; // 7
    static const int SRC_MASK_LEN = sizeof(in_addr_t);
    static const int SRC_ASID_POS = SRC_MASK_POS + SRC_MASK_LEN; // 11
    static const int SRC_ASID_LEN = sizeof(uint16_t);
    static const int DST_ADDR_POS = SRC_ASID_POS + SRC_ASID_LEN; // 13
    static const int DST_ADDR_LEN = sizeof(in_addr_t);
    static const int DST_MASK_POS = DST_ADDR_POS + DST_ADDR_LEN; // 17
    static const int DST_MASK_LEN = sizeof(in_addr_t);
    static const int DST_ASID_POS = DST_MASK_POS + DST_MASK_LEN; // 21
    static const int DST_ASID_LEN = sizeof(uint16_t);
    static const int RPRT_POS = DST_ASID_POS + DST_ASID_LEN; // 23
  };

  struct Control {
    static const int TYPE_POS = 0;
    static const int TYPE_LEN = 1;
    static const int TTL_POS = TYPE_POS + TYPE_LEN; // 1
    static const int TTL_LEN = sizeof(uint8_t);
    static const int SRC_ASN_POS = TTL_POS + TTL_LEN; // 2
    static const int SRC_ASN_LEN = sizeof(uint16_t);
    static const int SRC_PORT_POS = SRC_ASN_POS + SRC_ASN_LEN; // 4
    static const int SRC_PORT_LEN = sizeof(in_port_t);
    static const int SRC_ADDR_POS = SRC_PORT_POS + SRC_PORT_LEN; // 6
    static const int SRC_ADDR_LEN = sizeof(in_addr_t);
    static const int SRC_MASK_POS = SRC_ADDR_POS + SRC_ADDR_LEN; // 10
    static const int SRC_MASK_LEN = sizeof(in_addr_t);
    static const int RLY_ASN_POS = SRC_MASK_POS + SRC_MASK_LEN; // 14
    static const int RLY_ASN_LEN = sizeof(uint16_t);
    static const int RLY_PORT_POS = RLY_ASN_POS + RLY_ASN_LEN; // 16
    static const int RLY_PORT_LEN = sizeof(in_port_t);
    static const int RLY_ADDR_POS = RLY_PORT_POS + RLY_PORT_LEN; // 18
    static const int RLY_ADDR_LEN = sizeof(in_addr_t);
    static const int RLY_MASK_POS = RLY_ADDR_POS + RLY_ADDR_LEN; // 22
    static const int RLY_MASK_LEN = sizeof(in_addr_t);
    static const int DST_ASN_POS = RLY_MASK_POS + RLY_MASK_LEN; // 26
    static const int DST_ASN_LEN = sizeof(uint16_t);
    static const int DST_PORT_POS = DST_ASN_POS + DST_ASN_LEN; // 28
    static const int DST_PORT_LEN = sizeof(in_port_t);
    static const int DST_ADDR_POS = DST_PORT_POS + DST_PORT_LEN; // 30
    static const int DST_ADDR_LEN = sizeof(in_addr_t);
    static const int DST_MASK_POS = DST_ADDR_POS + DST_ADDR_LEN; // 34
    static const int DST_MASK_LEN = sizeof(in_addr_t);
    static const int TS_POS = DST_MASK_POS + DST_MASK_LEN; // 38
    static const int TS_LEN = sizeof(timeval);
    static const int BW_POS = TS_POS + TS_LEN; // 46
    static const int BW_LEN = sizeof(uint32_t);
    static const int TASK_POS = BW_POS + BW_LEN; // 50
    static const int TASK_LEN = sizeof(uint32_t);
    static const int CTRL_LEN = TASK_POS + TASK_LEN; // 54
  };
}

#endif
