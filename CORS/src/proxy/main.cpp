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

#include <string>
#include <unistd.h>
#include <iostream>
#include "../utils/types.h"
#include "../utils/utilfunc.h"
#include "corsproxy.h"
#include "neighbormaintainer.h"
#include "packetforwarder.h"
#include "relayadvisor.h"
#include "nbmtrfactory.h"
#include "fwdrfactory.h"
#include "rldvrfactory.h"
#include "simplenbmtr.h"

using namespace std;

void usage()
{
  cout << "usage: proxy [-o args]" << endl;
  cout << "-n: class name and parameters of neighbor maintainer, seperated by semecolon" << endl;
  cout << "-f: class name of packet forwarder, seperated by semecolon" << endl;
  cout << "-r: class name of relay advisor, seperated by semecolon" << endl;
  cout << "-p: minport-maxport, range of ports used by daemon" << endl;
}

int main(int argc, char *argv[])
{
  char ch, hyphen;
  string nbmtr = "SimpleNbmtr;nbmtr.conf;5;60;50;300;20;1200";
  string fwdr = "SimpleFwd";
  string rldvr = "AdvanceRldvr;166.111.137.46;16842";
  vector < string > nbmtr_params;
  vector < string > fwdr_params;
  vector < string > rldvr_params;
  uint16_t minp = 4097;
  uint16_t maxp = 8191;

  string delims = ";";
  int pos;
  while ((ch = getopt(argc, argv, "n:f:r:p:")) != -1) {
    switch (ch) {
    case 'n':
      nbmtr = optarg;
      break;
    case 'f':
      fwdr = optarg;
      break;
    case 'r':
      rldvr = optarg;
      break;
    case 'p':
      sscanf(optarg, "%hu%c%hu", &minp, &hyphen, &maxp);
      break;
    default:
      usage();
      exit(1);
    }
  }
  split(nbmtr, delims, nbmtr_params);
  split(fwdr, delims, fwdr_params);
  split(rldvr, delims, rldvr_params);

  cout << "Inintializing Corsproxy ..." << endl;

  NeighborMaintainer *pnbmtr =
      NbmtrFactory::getInstance()->create_nbmtr(nbmtr_params);
  if (pnbmtr) {
    cout << "Creating NeighborMaintainer Succeeds!" << endl;
  } else {
    exit(-1);
  }

  PacketForwarder *pfwdr =
      FwdrFactory::getInstance()->create_fwdr(fwdr_params);
  if (pfwdr) {
    cout << "Creating PacketForwarder Succeeds!" << endl;
  } else {
    exit(-1);
  }

  RelayAdvisor *prldvr =
      RldvrFactory::getInstance()->create_rldvr(rldvr_params);
  if (prldvr) {
    cout << "Creating RelayAdvisor Succeeds!" << endl;
  } else {
    exit(-1);
  }

  CorsProxy proxy(pnbmtr, pfwdr, prldvr, minp, maxp);
  proxy.start();
}
