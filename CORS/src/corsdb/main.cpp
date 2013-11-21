// Copyright (C) 2007 Zhen Chen <zhenchen3419@gmail.com>
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


#include "translator.h"


using namespace std;
using namespace cors;

uint16_t port;

void 
usage()
{
	cout << "usage: translator [-o args]" << endl;
	cout << "-h: help message" << endl;
	cout << "-d: run as daemon" << endl;
	cout << "-p: translator listing port" << endl;
}

int
main(int argc, char * argv[])
{
    string delims = ";";
    int pos;
    int ch;
/*
    while ((ch = getopt(argc, argv, "h:d:p:")) != -1) {
	switch (ch) {
		case 'p':
		    //port = optarg;
		    	sscanf(optarg, "%hu", &port);
		    break;
		default:
		    usage();
		    exit(1);
		}
    }
*/
    cout<<"run translator begin!"<<endl;

    Translator translator;
//    dispatcher();
    cout<<"returning"<<endl;

    return 1;

}
