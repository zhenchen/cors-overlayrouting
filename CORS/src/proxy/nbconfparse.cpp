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

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <arpa/inet.h>
#include "../utils/utilfunc.h"

using namespace std;

void
usage()
{
    cout << "usage: nbconfgparse inputfile outputfile" << endl;
}

int
main(int argc, char * argv[])
{
    ifstream ifs(argv[1]);
    if (!ifs) {
       cerr << "cannot open inputfile " << argv[1] << endl;
       exit(-1);
    }

    ofstream ofs(argv[2]);
    if (!ofs) {
       cerr << "cannot open outputfile " << argv[1] << endl;
       exit(-1);	
    }

    string line;
    vector<string> v;
    char buff[24];
    in_addr_t addr;
    in_port_t port;
    char type;
    double rtt;
    time_t ts;
    uint16_t asn;
    in_addr_t mask;
    while (getline(ifs, line)) {
       istringstream iss(line);
		iss >> type >> addr >> port >> rtt >> ts >> asn >> mask;
		if (type == 0) {
		    ofs << "oracle";
		    //cout << "oracle";
		} else {
		    ofs << "peer";
		    //cout << "peer";
		}

		ofs << " " << inet_ntop(AF_INET, &addr, buff, 24);
		ofs << " " << ntohs(port);
		ofs << " " << rtt;
		ofs << " " << ts;
		ofs << " " << asn;
		ofs << " " << inet_ntop(AF_INET, &mask, buff, 24) << endl;

		/*
		cout << " " << inet_ntop(AF_INET, &addr, buff, 24);
		cout << " " << ntohs(port);
		cout << " " << rtt;
		cout << " " << ts;
		cout << " " << asn;
		cout << " " << inet_ntop(AF_INET, &mask, buff, sizeof(mask)) << endl;
		*/
	}
    ifs.close();
    ofs.close();
}
