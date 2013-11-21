#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <vector>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <../client/corssocket.h>

using namespace std;

void usage()
{
	cout << "usage: simpleclient config_file" << endl;
}

int load_config(string file_name,
				Requirement &req,
				sockaddr_in &local,
				sockaddr_in &remote) 
{
	ifstream ifs(file_name.c_str());
	if (!ifs) {
		cerr << "Unable to open config file: " << file_name << endl;
		return -1;
	}
	string line;
	
	// load local port
	getline(ifs, line);
	istringstream iss1(line);
	in_port_t local_port;
	iss1 >> local_port;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(local_port);

	// load remote ip and port
	getline(ifs, line);
	istringstream iss2(line);
	in_port_t remote_port;
	in_addr_t remote_ip;
	string ip_str;
	iss2 >> ip_str >> remote_port;
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_addr.s_addr = inet_addr(ip_str.c_str());
	remote.sin_port = htons(remote_port);

	// load requirement
	getline(ifs, line);
	istringstream iss3(line);
	iss3 >> req.delay >> req.bandwidth >> req.lossrate >> req.qospara >> req.seculevel >> req.relaynum;

	return 0;
}

int init_sock(const sockaddr_in &local) 
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(sock, (sockaddr *)&local, sizeof(local)) < 0) {
		cerr << "socket error" << endl;
		close(sock);
		return -1;
	}
	return sock;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		usage();
		return -1;
	}
	string conf_file = argv[1];

	Requirement req;
	sockaddr_in local, remote;
	load_config(conf_file, req, local, remote);


	int sock = init_sock(local);
	if (sock < 0)
		return -1;

	vector<string> rsparams, mmparams;
	rsparams.push_back("SimpleRlysltr");
	rsparams.push_back("rsltr.conf");
	rsparams.push_back("1");
	rsparams.push_back("300");

	mmparams.push_back("SimpleMpathManager");
	cout << "pass" << endl;

	CorsSocket cors_socket(sock, rsparams, mmparams);
	cors_socket.init(req, remote.sin_addr.s_addr, remote.sin_port);

	int count = 0;
	Feature feature;
	feature.delay = 300;
	feature.significance = 0;
	feature.seculevel = 0;
	feature.channelnum = 3;

	while (count < 5) {
		stringstream ss;
		ss << "string: " << count << flush;
		cors_socket.sendto((char *)ss.str().c_str(), ss.str().length(), remote.sin_addr.s_addr, remote.sin_port, feature);
		cout << "Send " << ss.str() << endl;
		usleep(2000000);
		count++;
	}

	cors_socket.close();
}
