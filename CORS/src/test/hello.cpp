#include <iostream>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../utils/debug.h"
#include "../utils/utilfunc.h"
#include "../utils/ip2asn.h"
#include "../common/corsprotocol.h"

using namespace std;
using namespace cors;

int main() {
  sockaddr_in lo;
  lo.sin_family = AF_INET;
  lo.sin_port = htons(5000);
  lo.sin_addr.s_addr = inet_addr("216.165.108.69");

  DEBUG_PRINT(3, "lo: %s asn: %d\n", sockaddr2str(lo).c_str(), ntohs(ip2asn(lo.sin_addr.s_addr)));

  unsigned char msg[10];
  memset(msg, 1, 10);
  DEBUG_PRINT(3, "process packet(from %s)\n<--:%s\n",
              sockaddr2str(lo).c_str(),
              bytes2str(msg, 10).c_str());

  CorsProbe probe;
  probe.source_.asn = htons(1000);
  probe.source_.port = htons(5000);
  probe.source_.addr = inet_addr("166.111.137.21");
  probe.source_.mask = inet_addr("255.255.255.0");

  probe.destination_.asn = htons(2000);
  probe.destination_.port = htons(5010);
  probe.destination_.addr = inet_addr("216.165.108.69");
  probe.destination_.mask = inet_addr("255.255.0.0");

  probe.relay_.asn = htons(3000);
  probe.relay_.port = htons(3421);
  probe.relay_.addr = inet_addr("219.143.201.17");
  probe.relay_.mask = inet_addr("255.255.0.0");

  gettimeofday(&probe.timestamp_, NULL);
  probe.bandwidth_ = 1024;
  probe.load_num_ = 10;
  
  CorsProbe clone_probe(probe);

  int len = 0;
  uint8_t *buf = clone_probe.Serialize(len);
  DEBUG_PRINT(3, "probe: \n%s\n", bytes2str(buf, len).c_str());

  buf[0] = TYPE_FW_REPLY;
  buf[1]--;
  CorsProbe parse;
  int ret = parse.Deserialize(buf, len);
  if (ret < 0) {
    DEBUG_PRINT(1, "Deserialize() error\n");
  } else {
    DEBUG_PRINT(3, "%s\n", parse.ParseToString().c_str());
  }
  delete[] buf;

  CorsReport advice;
  advice.source_.asn = htons(1000);
  advice.source_.port = htons(5000);
  advice.source_.addr = inet_addr("1.1.1.1");
  advice.source_.mask = inet_addr("255.0.0.0");

  advice.destination_.asn = htons(2000);
  advice.destination_.port = htons(5010);
  advice.destination_.addr = inet_addr("2.2.2.2");
  advice.destination_.mask = inet_addr("255.255.0.0");

  ReportEntry entry;
  EndPoint relay;
  relay.asn = htons(3000);
  relay.port = htons(3421);
  relay.addr = inet_addr("3.3.3.3");
  relay.mask = inet_addr("255.255.255.0");
  entry.relay = relay;
  gettimeofday(&entry.delay, NULL);
  advice.reports_.push_back(entry);

  relay.asn = htons(4000);
  relay.port = htons(3421);
  relay.addr = inet_addr("4.4.4.4");
  relay.mask = inet_addr("255.255.255.0");
  entry.relay = relay;
  gettimeofday(&entry.delay, NULL);
  advice.reports_.push_back(entry);

  relay.asn = htons(5000);
  relay.port = htons(3421);
  relay.addr = inet_addr("5.5.5.5");
  relay.mask = inet_addr("255.255.255.0");
  entry.relay = relay;
  gettimeofday(&entry.delay, NULL);
  advice.reports_.push_back(entry);

  CorsReport clone_advice(advice);

  len = 0;
  buf = clone_advice.Serialize(len);
  DEBUG_PRINT(3, "Report: \n%s\n", bytes2str(buf, len).c_str());

  CorsReport parse2;
  ret = parse2.Deserialize(buf, len);
  if (ret < 0) {
    DEBUG_PRINT(1, "Deserialize() error\n");
  } else {
    DEBUG_PRINT(3, "%s\n", parse2.ParseToString().c_str());
  }
  delete[] buf;
}
