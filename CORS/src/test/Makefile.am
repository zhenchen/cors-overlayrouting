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
noinst_PROGRAMS = rtpsender rtpreceiver streamplayer streamdump

INCLUDES = $(CPPFLAGS)

rtpsender_SOURCES = rtpsender.cpp
rtpsender_LDFLAGS = -lpthread
rtpsender_LDADD = libcorssocket.a libUtils.a libCommon.a

rtpreceiver_SOURCES = rtpreceiver.cpp
rtpreceiver_LDFLAGS = -lpthread
rtpreceiver_LDADD = libcorssocket.a libUtils.a libCommon.a

streamplayer_SOURCES = streamplayer.cpp jitter.cpp
streamplayer_LDFLAGS = -lpthread
streamplayer_LDADD = libcorssocket.a libUtils.a libCommon.a

streamdump_SOURCES = streamdump.cpp jitter2.cpp
streamdump_LDFLAGS = -lpthread
streamdump_LDADD = libcorssocket.a libUtils.a libCommon.a
