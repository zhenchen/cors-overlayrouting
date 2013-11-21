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


#include "eventqueue.h"
#include <vector>
#include <iostream>
using namespace std;

EventQueue* EventQueue::instance = NULL;

EventQueue*
EventQueue::Instance()
{
  if (instance)
    return instance;
  return (instance = new EventQueue());
}

EventQueue::~EventQueue()
{
  entry* next = 0;
  entry* cur = queue.first();
  while(cur) {
    for (vector<Event*>::iterator it = cur->events.begin(); it != cur->events.end(); ++it) {
      delete (*it);
    }
    next = queue.next(cur);
    delete cur;
    cur = next;
  }
}

void
EventQueue::run()
{
  entry * etr = queue.first();
  time_t now = time(NULL);
  while ((etr != NULL) && (etr->ts <= now)) {
    queue.remove(etr->ts);
    for (vector<Event*>::iterator it = etr->events.begin(); it != etr->events.end(); ++it) {
      (*it)->execute();
      delete (*it);
    }
    delete etr;	
    etr = queue.first();	
    now = time(NULL);
  }
}

time_t
EventQueue::get_idle_time()
{
  entry * etr = queue.first();
  if (etr == NULL) {
    return 60;
  } else {
    Time t = etr->ts - time(NULL);
    return (t > 0) ? t : 0;
  }
}

void
EventQueue::add_event(Event* e)
{
  //assert(e->ts >= clock);
  if (e->ts < clock) {
    delete e;
    return;
  }
  entry* etr = queue.search(e->ts);
  if (!etr) {
    etr = new entry(e);
    assert(etr);
    bool ok = queue.insert(etr);
    assert(ok);
  }
  etr->events.push_back(e);
}

void
EventQueue::append_event(Event* e)
{
  entry* etr = queue.last();
  if (etr) {
    e->set_ts(etr->ts + 1);
  } else {
    e->set_ts(clock + 1);
  }
  add_event(e);
}
