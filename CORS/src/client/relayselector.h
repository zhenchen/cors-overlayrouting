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

#ifndef __CLIENT_RELAYSELECTOR_H
#define __CLIENT_RELAYSELECTOR_H

#include <sys/socket.h>
#include <string>
#include <vector>
#include "corssocket.h"

/** Abstract base class of RelaySelector
 */
class RelaySelector {
 public:
  /** constructor
   * \param sk pointer to a CorsSocket object
   */
  RelaySelector(CorsSocket * sk) : psk(sk) {}
  /** send an INQUIRY packet according to requirement
   * \param rq Requirement of path performance 
   * \param sk descriptor of a socket
   */
  virtual void send_rl_inquiry(const Requirement &rq, int sk) = 0;
  /** process an ADVICE packet, get relay node candidates from the packet and add them to the list of relay node candidates
   * \param msg pointer to the buffer of the packet
   * \param len length of packet
   * \param peeraddr filled with the address of where the packet come from
   * \param sk descriptor of socket
   */
  virtual void process_rl_advice(char *msg, int len, sockaddr_in &peeraddr, int sk) = 0;
  /** (As the destination node) process the PROBE packet from source or relay
   */
  virtual void process_fw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk) = 0;
  /** (As the source node) process the response of PROBE from destination
   */ 
  virtual void process_fw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk) = 0;
  /** (As the destination node) process the RESERVE packet from relay
   */
  virtual void process_bw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk) = 0;
  /** (As the source node) process the response of RESERVE from destination
   */
  virtual void process_bw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk) = 0;
  /** send RELEASE packets to all in-used relays
   */
  virtual void release_resources(int sk) = 0;
  /** copy relay node list to rn_list (for MultipathManager to access the relay node list separately)
   */
  virtual void get_relaylist(list<RelayNode> &rn_list) = 0;
  /** set the member req to the value of param rq
   */
  virtual void set_requirement(const Requirement &rq) = 0;
  /** fill rq with the value of member req
   */
  virtual void fill_requirement(Requirement &rq) = 0;
  
  /** pointer to the CorsSocket object, for accessing other components like MultipathManager
   */  
  CorsSocket * psk;
};

#endif
