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

#include <vector>
#include <string>
#include <stdexcept>
#include "../utils/debug.h"
#include "mmgrfactory.h"

using namespace std;

MmgrFactory * MmgrFactory::instance = new MmgrFactory();

MmgrFactory *
MmgrFactory::getInstance()
{
    return instance;
}

MmgrFactory::MmgrFactory()
{
}

MultipathManager *
MmgrFactory::create_mmgr(CorsSocket * corssk, vector<string> & param)
{
	try {
		if (param[0] == "") {
	    // to do
	    return NULL;
		} else if (param[0] == "SimpleMpathManager"){
   		return new SimpleMpathManager(corssk); 
   	}	else if (param[0] == "StatMpathManager"){
			param.erase(param.begin());
   		return new StatMpathManager(corssk, param); 
		} else {
	    DEBUG_PRINT(2, "invalid name of multipath manager to be created\n");
	    return NULL;
		}
	} catch (const runtime_error & e) {
		DEBUG_PRINT(2, (string(e.what()) + "\n").c_str());
		return NULL;
   }
}
