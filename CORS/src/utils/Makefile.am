# Copyright (C) 2007 Li Tang <tangli99@gmail.com>
#  
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#  
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#  
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#  

noinst_LIBRARIES = libUtils.a
noinst_HEADERS = unpifi.h utilfunc.h ip2asn.h
libUtils_a_SOURCES = get_ifi_info.c utilfunc.cpp ip2asn.cpp

noinst_PROGRAMS = convertor_ip2asn
INCLUDES = $(CPPFLAGS)
convertor_ip2asn_SOURCES = ip2asn.h ip2asn.cpp convertor_ip2asn.cpp