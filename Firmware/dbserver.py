'''
    Simple socket server using threads
'''

import socket
import sys
from thread import *
import json
import argparse
import sqlite3
from select import select
import threading

HOST = '127.0.0.1'   # Symbolic name meaning all available interfaces
PORT = 8888 # Arbitrary non-privileged port

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print 'Socket created'

#Bind socket to local host and port
try:
    s.bind((HOST, PORT))
except socket.error as msg:
    print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
    sys.exit()

print 'Socket bind complete'

#Start listening on socket
s.listen(10)
s.settimeout(0.01) ## ~10ms
print 'Socket now listening'

class Server_stopper(threading.Thread):
    # maxRetries = 20
    def __init__(self, thread_id, name, lock, kill):
        threading.Thread.__init__(self)
        self.thread_id = thread_id
        self.name = name
        self.lock = lock
        self.kill = kill

    def run(self):
        print "1Starting " + self.name
        timeout = 0
        while not self.kill.is_set():
            rlist,_,_ = select([sys.stdin], [], [], timeout)
            if rlist:
                key = sys.stdin.read(1)
                if key == 'q':
                    # print "STOP"
                    self.kill.set()
                    break
        print "Exiting started..."

class Server_thread(threading.Thread):
    # maxRetries = 20
    def __init__(self, thread_id, name, conn, lock, kill):
        threading.Thread.__init__(self)
        self.thread_id = thread_id
        self.name = name
        self.conn = conn
        self.lock = lock
        self.kpill = kill

    def run(self):
        print "2Starting " + self.name
        self.conn.send('Welcome to the server. Type something and hit enter\n') #send only takes string
        sqlcon = sqlite3.connect('test.sqlite3')
        sqlcursor = sqlcon.cursor()

        #infinite loop so that function do not terminate and thread do not end.
        while not self.kpill.is_set():

            #Receiving from client
            data = self.conn.recv(1024)
            reply = 'OK...' + data
            if not data:
                break
            jdata = json.loads(data)
            self.conn.sendall(reply)
            sqlcursor.execute('''insert into data_leddar (sensorID, objectID, x, y, type, x_vel, y_vel, x_next, y_next) values (?,?,?,?,?,?,?,?,?)''', (jdata['sensorID'], jdata['objectID'], jdata['x'], jdata['y'], jdata['type'], jdata['x_vel'], jdata['y_vel'], jdata['x_next'], jdata['y_next'] ))
            #sqlcursor.execute('''insert into data_leddar (sensorID, objectID, mstimestamp, distance, amplitude, x, y, type) values (?,?,?,?,?,?,?,?)''', (jdata['sensorID'], jdata['objectID'], jdata['mstimestamp'], jdata['distance'],jdata['amplitude'], jdata['x'], jdata['y'], jdata['type']))
            #sqlcursor.execute('''insert into testdata (sensor, command, data) values (?,?,?)''', (jdata['sensor'], jdata['command'], jdata['data']))
            sqlcon.commit()
        #came out of loop
        self.conn.close()
        sqlcon.commit()
        sqlcon.close()
        print "Exiting completed..."


# def thread_stopper(kpill):#,tlock):
#     timeout = 0
#     while not kpill.is_set():
#         rlist,_,_ = select([sys.stdin], [], [], timeout)
#         if rlist:
#             key = sys.stdin.read(1)
#             if key == 'q':
#                 kpill.set()
#                 break
#     print "exiting..."

#Function for handling connections. This will be used to create threads
# def clientthread(conn,kpill):#,tlock):
#     #Sending message to connected client
#     conn.send('Welcome to the server. Type something and hit enter\n') #send only takes string
#     sqlcon = sqlite3.connect('test.sqlite3')
#     sqlcursor = sqlcon.cursor()
#
#     #infinite loop so that function do not terminate and thread do not end.
#     while not kpill.is_set():
#
#         #Receiving from client
#         data = conn.recv(1024)
#         reply = 'OK...' + data
#         if not data:
#             break
#         jdata = json.loads(data)
#         if not jdata['data'] == "test":
#             conn.sendall(reply)
#             sqlcursor.execute('''insert into testdata (sensor, command, data) values (?,?,?)''', (jdata['sensor'], jdata['command'], jdata['data']))
#             sqlcon.commit()
#         else:
#             conn.sendall("HA HA!")
#     #came out of loop
#     print "exiting?"
#     conn.close()
#     sqlcon.commit()
#     sqlcon.close()

sqlcon = sqlite3.connect('test.sqlite3')
sqlcursor = sqlcon.cursor()
sqlcursor.execute('''create table if not exists data_leddar (id integer primary key autoincrement, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, sensorID text not null, objectID int not null, x real not null, y real not null, type int not null, x_vel real not null, y_vel real not null,  x_next real not null,  y_next real not null ) ''')
#sqlcursor.execute('''create table if not exists data_leddar (id integer primary key autoincrement, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, sensorID text not null, objectID int not null, mstimestamp int not null, distance real not null, amplitude real not null, x int not null, y real not null, type int not null ) ''')
#sqlcursor.execute('''create table if not exists testdata (id integer primary key autoincrement, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, sensor int not null, command int not null, data varchar(255)) ''')
sqlcon.commit()
sqlcursor.close()
sqlcon.close()

killpill = threading.Event()
thread_lock = threading.RLock()
threads = []
threadId = 0
thread = Server_stopper(threadId, "stopper", thread_lock, killpill)
thread.start()
threads.append(thread)
threadId += 1

# start_new_thread(thread_stopper, (killpill,))#thread_lock,))

#now keep talking with the client
while not killpill.is_set():
    #wait to accept a connection - blocking call
    # print "test..."
    try:
        conn, addr = s.accept()
        print 'Connected with ' + addr[0] + ':' + str(addr[1])

        #start new thread takes 1st argument as a function name to be run, second is the tuple of arguments to the function.
        # start_new_thread(clientthread ,(conn,killpill,))#thread_lock,))
        thread = Server_thread(thread, "server", conn, thread_lock, killpill)
        thread.start()
        threads.append(thread)
        threadId += 1
    except socket.timeout:
        if len(threads) == 0:
            break
        for t in threads:
            if t.isAlive() == False:
                threads.remove(t)
    # threadId += 1

for t in threads:
    t.join()
s.close()
