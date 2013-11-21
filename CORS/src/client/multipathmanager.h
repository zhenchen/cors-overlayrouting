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

#ifndef __CLIENT_MULTIPATHMANAGER_H
#define __CLIENT_MULTIPATHMANAGER_H

#include <vector>
#include "corssocket.h"

/** abstract base class of MultipathManger
 */
class MultipathManager {
public:
    /** constructor
     * \param sk pointer to a CorsSocket object
     */
    MultipathManager(CorsSocket *sk) : psk(sk) {};
    virtual int add_relay(RelayNode & r) = 0;
    /** send a packet
     * \param buff pointer to the buffer of a packet
     * \param nbytes length of packet
     * \param dst destination socket address
     * \param ft packet Feature
     * \param sk descriptor of socket
     * \return number of bytes sent out
     */
    virtual int send_packet(char * buff, int nbytes, const sockaddr_in & dst, const Feature & ft, int sk) = 0;
    /** parse a data packet (packet with a TYPE of TYPE_DATA)
     * strip the CORS headers and return payload to upper layer
     * \param buff pointer to a buffer where payload of the packet to be stored
     * \param bufsize filled with the length of payload
     * \param saddr filled with the source ip address
     * \param sport filled with the source port
     * \param peeraddr filled with address of where the packet come from (relay node or source node)
     * \param sk descriptor of socket
     * \param flag filled with a mark indicating direct or indirect path (if flag is not NULL)
     * \return pointer to the buffer
     */
    virtual char * parse_data_packet(char * buff, int & bufsize, in_addr_t & saddr, in_port_t & sport, sockaddr_in &peeraddr, int sk, int * flag) = 0;
    /** process a path performance statistics packet (TYPE_STAT)
     * \param msg pointer to the buffer of the control packet
     * \param nbytes length of the control packet
     * \param peer peer address
     * \param sk descriptor of socket
     */
    virtual void process_path_statistics(char * msg, int nbytes, sockaddr_in & peer, int sk) = 0;
    /** check if Requirement is satisfied */
    virtual bool get_requirement(Requirement & rq) = 0;
    /** release resources */
    virtual void release_resources(int sk) = 0;
    /** pointer to the CorsSocket object */
    CorsSocket * psk;
};

#endif
