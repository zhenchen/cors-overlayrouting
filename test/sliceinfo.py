#!/usr/bin/python

import xmlrpclib
import popen2
import sys
import os

output = open('log.out', 'w')
error = open('log.err', 'w')

server = xmlrpclib.Server("https://www.planet-lab.org/PLCAPI/")

slicename = "cernettsinghua_xyy"

auth = {}
auth['AuthMethod'] = 'password'
auth['Username'] = 'zhenchen@tsinghua.edu.cn'
auth['AuthString'] = 'security421'

attrs = server.GetSliceAttributes(auth)
print attrs

# server.UpdateSliceAttribute(auth, 20019, '1')
slices = server.SliceInfo(auth)
for slice in slices:
  print "Slice: %20s"%slice['name']
  print "Users: ", slice['users']
  print "Expires: %s"%slice['expires']
  print "Contains %d nodes"%len(slice['nodes'])
  for node in slice['nodes']:
    print '======', node, '======'
    cmd = 'ssh %s@%s uname -n'%(slice['name'], node)
    p = popen2.Popen3(cmd, capturestderr=True)
    p.wait()
    # print p.fromchild.read()
    # print p.childerr.read()
    # output.write('====== %s ======\n'%node)
    output.write(p.fromchild.read())
    output.flush()
    error.write('====== %s ======\n'%node)
    error.write(p.childerr.read())
    error.flush()
    # os.system('ssh %s@%s uname -n'%(slice['name'], node))

