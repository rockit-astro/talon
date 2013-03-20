#!/usr/bin/env python
import sys
import os
import paramiko
import hashlib
import select
import math
import commands

#This scipt accepts two arguments, filename and, if its the last calibration file, "last": 
if len(sys.argv) > 1:
	fname = sys.argv[1]
else:
	fname = 'default' 

if len(sys.argv) > 2:
	last = sys.argv[2]
else:
	last = 'not'

hostname = 'alis' # remote hostname where SSH server is running
port = 22
username = 'talon'
#password = ''

dname = commands.getoutput('readkeyword '+fname+' JD')
dname = str(int(math.floor(eval(dname))))

dir_local='/home/optjo/fits/'
dir_remote ='/mnt/storage/rawdata/'+dname

print '=' * 60
files_copied = 0
# now, connect and use paramiko Transport to negotiate SSH2 across the connection
try:
    print 'Establishing SSH connection to:', hostname, port, '...'
    print '=' * 60
    t = paramiko.Transport((hostname, port))
    #t.connect(username=username, password=password)
    t.connect(username=username, pkey = paramiko.RSAKey.from_private_key_file(filename='/home/optjo/.ssh/id_rsa'))
    sftp = paramiko.SFTPClient.from_transport(t)

    #If this is the first image of the day, create a new folder with JD..
    #Always check if directory exists, if not, create it
    try:
        sftp.mkdir(dir_remote)
        print 'Created:',dir_remote
    except IOError, e:
        print 'Exists: ',dir_remote


    print 'Copying: '+fname
    is_up_to_date = False
   
    local_file = os.path.join(dir_local, fname)
    remote_file = dir_remote + '/' + os.path.basename(fname)
    print '=' * 60
    #if remote file exists
    try:
        if sftp.stat(remote_file): 
            local_file_data = open(local_file, "rb").read()
            remote_file_data = sftp.open(remote_file).read()
            md1 = hashlib.sha224(local_file_data).hexdigest()
            md2 = hashlib.sha224(remote_file_data).hexdigest()
                    
            if md1 == md2:
                is_up_to_date = True
                print os.path.basename(fname)," exists and has not been changed"
            else:
                is_up_to_date = False
                print os.path.basename(fname)," exists but has different md5 value, overwriting..."
    except:
        print "NEW: ", os.path.basename(fname)

    #if remote file doesn't exist
    if not is_up_to_date:
        print 'Copying', local_file, 'to ', remote_file
        sftp.put(local_file, remote_file)
        local_file_data = open(local_file, "rb").read()
        remote_file_data = sftp.open(remote_file).read()
        md1 = hashlib.sha224(local_file_data).hexdigest()
        md2 = hashlib.sha224(remote_file_data).hexdigest()
                 
        if md1 == md2:
            is_up_to_date = True
            print os.path.basename(fname),"copied correctly"
            os.remove(local_file)
            print os.path.basename(fname),"removed"
        else:
            print os.path.basename(fname),"not copied correctly"
            
        files_copied += 1
        
#    t.close()

except Exception, e:
    print '*** Caught exception: %s: %s' % (e.__class__, e)
    try:
        t.close()
    except:
        pass
print '=' * 60



#**************************

def run(t, cmd):
	#'Open channel on transport, run command, capture output and return'
	out = ''

	chan = t.open_session()
	chan.setblocking(0)
        chan.exec_command(cmd)

	### Read when data is available
	while select.select([chan,], [], []):
		x = chan.recv(1024)
		if not x: break
		out += x
		select.select([],[],[],.1)
	
	chan.close()
	return out

### Unbuffered sys.stdout
sys.stdout = os.fdopen(1, 'w', 0)

fname2 = fname.split('/')


if last == 'last':
	print 'Starting final processing for calibration images'
	runout = run(t,'python /opt/dataprocessing/dataprocess.py '+dname+' '+fname2[-1]+' last')
else:
	print 'Starting data processing'
	runout = run(t,'python /opt/dataprocessing/dataprocess.py '+dname+' '+fname2[-1])

status = runout.count('START')

print dname+' '+fname2[-1]

if status==1:
	print 'Data processing started successfully'
else:
	print 'Data processing failed to start'

print '=' * 60

t.close()
