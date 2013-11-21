#!/usr/bin/python

from xmlrpclib import *
from urllib import urlopen
import sys
import string

server_url = "https://www.planet-lab.org/PLCAPI/"
server= Server(server_url)

## CUSTOMIZE HERE ##
slice = 'my_slice_name'

auth= {}
auth['AuthMethod']="password"
auth['Username']="my_user_account"
auth['AuthString']="my_password"
auth['Role']="user"
## CUSTOMIZE HERE ##

def get_node_list(cat):
    if cat not in ['alpha', 'beta', 'production', 'all']:
        raise "bad category %s" % cat
    fh = file('all_hosts')
    line = ' '
    hosts = []
    line = fh.readline()
    while line != '':
        line = string.strip(line)
        hosts.append(line)
        line = fh.readline()
    return hosts

def get_slice_info(srv, auth, slicename):
    slices = srv.SliceInfo(auth)
    for s in slices:
        if s['name'] == slicename:
            return s
    raise "slice %s not found" % slicename

def print_slice_info(s):
    print "Slice: %20s Expires: %s" % (s['name'], s['expires'])
    print "Users: ", s['users']
    print "Contains %d nodes" % len(s['nodes'])
    print s['nodes']
    print
         
try:
    slicerec = get_slice_info(server, auth, slice)
    all_nodes = get_node_list('all');
    # The system doesn't drop duplicates automatically, so manually suppress them.
    for node in slicerec['nodes']:
        if node in all_nodes:
            all_nodes.remove(node)
    if len(all_nodes) > 0:
        print all_nodes
        r = server.SliceNodesAdd(auth, slice, all_nodes)
        print "Add to new nodes: %s" %r
    else:
        print "No new nodes to add!"

    slicerec = get_slice_info(server, auth, slice)
    print_slice_info(slicerec)

except Error, err:
    print "XML-RPC Error: ", err
