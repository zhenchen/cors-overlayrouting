#ifndef __CLIENT_SIMPLERLYSLTR_H
#define __CLIENT_SIMPLERLYSLTR_H

#include <pthread.h>
#include <signal.h>

#include <vector>
#include <string>
#include <list>

#include "relayselector.h"
#include "../proxy/relayunit.h"
#include "../proxy/eventqueue.h"
#include "../common/corsprotocol.h"
#include "../utils/utilfunc.h"
#include "../utils/debug.h"

using namespace std;
using namespace cors;

const int MAX_CAND_LEN = 20;

/** A simple implementation of RelaySelector
 */
class SimpleRlysltr : public RelaySelector {
 public:
  /** 1st param -- configuration file name \n
   *  2nd param -- interval of probing \n
   *  3rd param -- expiration time of an unalive relay node
   */
  enum PARAM_INDEX { CONFIG, PROBE_INTERVAL, TIME_OUT };
  /** constructor
   * \param sk pointer to a CorsSocket object
   * \param param specific information for constructing SimpleRlysltr
   */
  SimpleRlysltr(CorsSocket * sk, std::vector<std::string> &param);
  /** destructor
   */
  virtual ~SimpleRlysltr();

  // virtual void send_candidates_request(const Requirement &rq, int sk);
  /** send an INQUIRY packet according to requirement
   * \param rq Requirement of path performance 
   * \param sk descriptor of a socket
   */
  virtual void send_rl_inquiry(const Requirement &rq, int sk);
  /** process an ADVICE packet, get relay node candidates from the packet and add them to the list of relay node candidates
   * \param msg pointer to the buffer of the packet
   * \param len length of packet
   * \param peeraddr filled with the address of where the packet come from
   * \param sk descriptor of socket
   */  
  virtual void process_rl_advice(char *msg, int len, sockaddr_in &peeraddr, int sk);
  /** (As the destination node) process the PROBE packet from source or relay
   */
  virtual void process_fw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk);
  /** (As the source node) process the response of PROBE from destination
   */ 
  virtual void process_fw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk);
  /** (As the destination node) process the RESERVE packet from relay
   */
  virtual void process_bw_forward(char *msg, int len, sockaddr_in &peeraddr, int sk);
  /** (As the source node) process the response of RESERVE from destination
   */
  virtual void process_bw_reply(char *msg, int len, sockaddr_in &peeraddr, int sk);
  /** send RELEASE packets to all in-used relays
   */
  virtual void release_resources(int sk);
  /** copy relay node list to rn_list (for MultipathManager to access the relay node list separately)
   */
  virtual void get_relaylist(list<RelayNode> &rn_list);
  /** set the member req to the value of param rq
   */
  virtual void set_requirement(const Requirement &rq)
  { memcpy(&req, &rq, sizeof(req)); }
  /** fill rq with the value of member req
   */  
  virtual void fill_requirement(Requirement &rq) 
  { memcpy(&rq, &req, sizeof(req)); }

  /** \return iterator to the relay node list
   */
  list<RelayNode>::iterator get_iter_relaylist()
  { return iter_relaylist; } 
  /** \return iterator to the relay candidate list
   */
  list<EndPoint>::iterator get_iter_candidates()
  { return iter_candidates; } 
  /** increase iterator (of the two lists) circularly in probing
   */ 
  void increase_iter();

 private:
  /** thread routine of circular probing
   * \param pParam pointer to the SimpleRlysltr object
   */
  static void * cycle_ping(void * pParam);

  /** check if delay can meet requirement
   */
  bool meet_requirement(const timeval &tm_delay);

  /** \return the interval of probing
   */
  time_t get_probe_interval() { return probe_interval_; }
  /** \return the value of expiration time
   */
  time_t get_time_out() { return time_out_; }

 private:
  /** configuration file name
   */
  std::string config_;
  /** interval of probing
   */
  time_t probe_interval_;
  /** expiration time (not in use now)
   */
  time_t time_out_;
  /** time of sending the last REPORT packet
   */
  time_t last_report_time_;
  /** a flag indicating if the main thread is running. 
   * \\If it's stopped, we terminate the cycle_ping thread.
   */
  bool running_;
  /** thread id of cycle_ping
   */
  pthread_t cycle_tid_;
  // RluList candidates_;
  /** relay candidate list */
  list<EndPoint> candidates_;
  /** iterator of relay candidate list */
  list<EndPoint>::iterator iter_candidates;
  // RluList::iterator iter_candidates;
  /** mutex for thread safety of relay candidate list */
  pthread_mutex_t candidates_mutex_;

  /** relay node list */
  list<RelayNode> relaylist_;
  // list<RelayNode>::iterator curr_rly_;
  /** iterator of relay node list */
  list<RelayNode>::iterator iter_relaylist;
  // RluList relaylist_;
  /** mutex for thread safety of relay node list */
  pthread_mutex_t relaylist_mutex_;
 
  Requirement req;
};
#endif
