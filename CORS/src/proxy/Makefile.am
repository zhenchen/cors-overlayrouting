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

#SUBDIRS = tools

noinst_PROGRAMS = corsproxy nbconfgen nbconfparse

INCLUDES = $(CPPFLAGS)

corsproxy_SOURCES = corsproxy.h corsproxy.cpp \
		    eventqueue.h eventqueue.cpp \
		    neighbormaintainer.h packetforwarder.h relayadvisor.h \
		    nbmtrfactory.h nbmtrfactory.cpp \
		    fwdrfactory.h fwdrfactory.cpp \
		    rldvrfactory.h rldvrfactory.cpp \
		    simplenbmtr.h simplenbmtr.cpp \
		    simplefwd.h simplefwd.cpp \
				simplerldvr.h simplerldvr.cpp \
				advancerldvr.h advancerldvr.cpp \
				main.cpp
# advancerldvr.h advancerldvr.cpp 

corsproxy_LDFLAGS = -lpthread

corsproxy_LDADD = ../utils/libUtils.a ../common/libCommon.a
# ../common/libCommon.a

nbconfgen_SOURCES = nbconfgen.cpp

nbconfgen_LDADD = ../utils/libUtils.a

nbconfparse_SOURCES = nbconfparse.cpp
nbconfparse_LDADD = ../utils/libUtils.a
