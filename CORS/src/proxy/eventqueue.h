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

#ifndef __EVENTQUEUE_H
#define __EVENTQUEUE_H

#include "skiplist.h"
#include <time.h>
#include <vector>

typedef time_t Time;

class Event {
    friend class EventQueue;
public:
    Event(Time t) : ts(t) {}
    virtual ~Event() {}
    virtual void execute() = 0;
    Time get_ts() { return ts; }
    void set_ts(Time t) { ts = t; }
protected:
    Time ts;    
};

class EventQueue {
public:
    static EventQueue* Instance();
    ~EventQueue();

    void run();
    time_t get_idle_time();
    void add_event(Event*);
    void append_event(Event*);
    Time curr_time() { return clock; }

private:
    EventQueue() : clock(0) {}
    static EventQueue* instance;
    
    struct entry {
	entry() : ts(0) {}
	entry(Event* e) : ts(e->get_ts()) {}
	Time ts;
	std::vector<Event*> events;
	sklist_entry<entry> softlink;
    };
    skiplist<entry, Time, &entry::ts, &entry::softlink> queue;
    Time clock;    
};

#endif
