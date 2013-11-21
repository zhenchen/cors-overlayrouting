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

#ifndef __PROXY_NBMTRFACTORY_H
#define __PROXY_NBMTRFACTORY_H

#include <vector>
#include <string>
#include "neighbormaintainer.h"

class NbmtrFactory {
public:
    static NbmtrFactory * getInstance();
    NeighborMaintainer * create_nbmtr(std::vector<std::string> & param);

private:
    static NbmtrFactory * instance;

    NbmtrFactory();
};

#endif
