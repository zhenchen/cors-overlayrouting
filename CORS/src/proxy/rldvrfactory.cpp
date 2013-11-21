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

#include "rldvrfactory.h"
#include "../utils/debug.h"
#include "simplerldvr.h"
#include "advancerldvr.h"

using namespace std;

RldvrFactory *RldvrFactory::instance = NULL;

RldvrFactory *RldvrFactory::getInstance()
{
  if (instance == NULL) {
    instance = new RldvrFactory();
  }

  return instance;
}

RldvrFactory::RldvrFactory()
{
}

RelayAdvisor *RldvrFactory::create_rldvr(vector<string> &param)
{
  if (param[0] == "SimpleRldvr") {
    param.erase(param.begin());
    return new SimpleRldvr(param);
  } else if (param[0] == "AdvanceRldvr"){
    param.erase(param.begin());
    return new AdvanceRldvr(param);
  } else {
    DEBUG_PRINT(2, "invalid name of relay advisor to be created\n");
    return NULL;
  }
}
