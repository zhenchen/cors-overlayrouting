/*
** Copyright (C) 2006 Li Tang <tangli99@gmail.com>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

/* Originating from Unix Network Programming Volume 1 (3rd Edition) */

#ifndef	__unp_ifi_h
#define	__unp_ifi_h

#ifdef __cplusplus
extern "C" {
#endif
    
#include	<net/if.h>

#define	IFI_NAME	16	/* same as IFNAMSIZ in <net/if.h> */
#define	IFI_HADDR	 8	/* allow for 64-bit EUI-64 in future */

struct ifi_info {
  char    ifi_name[IFI_NAME];	/* interface name, null-terminated */
  short   ifi_index;		/* interface index */
  short   ifi_mtu;		/* interface MTU */
  u_char  ifi_haddr[IFI_HADDR];	/* hardware address */
  u_short ifi_hlen;		/* # bytes in hardware address: 0, 6, 8 */
  short   ifi_flags;		/* IFF_xxx constants from <net/if.h> */
  short   ifi_myflags;		/* our own IFI_xxx flags */
  struct sockaddr  *ifi_addr;	/* primary address */
  struct sockaddr  *ifi_mask;   /* subnet mask */
  struct sockaddr  *ifi_brdaddr;/* broadcast address */
  struct sockaddr  *ifi_dstaddr;/* destination address */
  struct ifi_info  *ifi_next;	/* next of these structures */
};

#define	IFI_ALIAS	1	/* ifi_addr is an alias */

				/* function prototypes */
struct ifi_info	*get_ifi_info(int, int);
struct ifi_info	*Get_ifi_info(int, int);


#ifdef __cplusplus
}
#endif

#endif	/* __unp_ifi_h */
