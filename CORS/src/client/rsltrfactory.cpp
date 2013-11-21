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
#include "rsltrfactory.h"
#include "../utils/debug.h"

using namespace std;

RsltrFactory * RsltrFactory::instance = new RsltrFactory(); // to avoid multi-thread problems 

RsltrFactory *
RsltrFactory::getInstance()
{
    return instance;
}

RsltrFactory::RsltrFactory()
{
}

RelaySelector *
RsltrFactory::create_rsltr(CorsSocket * corssk, vector<string> & param)
{
    try {
		if (param [0] == "") {
	    	// to do
	    	return NULL;
		} else if (param[0] == "SimpleRlysltr") {
			param.erase(param.begin());
			return new SimpleRlysltr(corssk, param);
		} else {
	    	DEBUG_PRINT(2, "invalid name of relay selector to be created\n");
	    	return NULL;
		}
    } catch (const runtime_error & e) {
		DEBUG_PRINT(2, (string(e.what()) + "\n").c_str());
		return NULL;
    }
}
