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

lib_LTLIBRARIES = libcorssocket.la
libcorssocket_la_SOURCES = corssocket.h corssocket.cpp \
			   relayselectory.h rsltrfactory.h rsltrfactory.cpp \
			   multipathmanager.h mmgrfactory.h mmgrfactory.cpp \
			   simplempathmanager.h simplempathmanager.cpp \
			   statmpathmanager.h statmpathmanager.cpp \
			   simplerlysltr.h simplerlysltr.cpp 

libcorssocket_la_LIBADD = ../common/libCommon.a ../utils/libUtils.a
libcorssocket_la_LDFLAGS = -version-info 0:3 -release 0:3

library_includedir = $(includedir)/cors
library_include_HEADERS = types.h corsprotocol.h corssocket.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = corssocket.pc
