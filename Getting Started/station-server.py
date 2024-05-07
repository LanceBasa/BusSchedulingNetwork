#Needs to start server from this .py
#Invokes this way: ./station-server.py [station name] [TCP port] [UDP port] [neighbour(s)]

#accept query from tcp port and bidirectional from UDP port 
#call timetable.py to get fastest times to reach my neighbours --> in the form of --> 
#---> (APPEND DESTINATION HERE BECAUSE WE GET THIS FROM HTML) Departure time, route name, departing from, arrival time, arrival station 
#
#./stn-server.py; timetable(tt-busportB, currenttime [which is given to us by html])
#./stn-server.py; dict timetable = timetable.py(timetablename, currenttime);
#
# if destination in timetable: return constructed query back to html
# else: send query to all neighbours 

# output_to_html = "Catch Bus-N from station-A at time HH:MM, to arrive at station-B at time HH:MM"
# query_format = "[Destination], [station], [departure time], [arrival time], ..."

# What to send to neighbours???? We were going to just use another query which was similar to just doing a TCP/IP request to neighbour


# UDP send
# UDP receive
# TCP send
# TCP receive

import socket
import sys
import re
import select
import os
import datetime

num_neighbors = len(sys.argv) - 3
neighbor_dictionary=[]

def tcp_server(tcp_port):
    # Define the host
    host = 'localhost'  # Listen on localhost
    
    # Create a TCP socket object
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        # Bind the socket to the host and port
        tcp_socket.bind((host, tcp_port))
        
        # Start listening for incoming connections
        tcp_socket.listen(5)
        print(f"TCP server listening for incoming connections on port {tcp_port}...")
        
        return tcp_socket
    
    except OSError as e:
        print(f"Error binding TCP socket: {e}")
        sys.exit(1)

def udp_server(udp_port, neighbors=None):
    # Define the host
    host = 'localhost'  # Listen on localhost
    
    # Create a UDP socket object
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        # Bind the socket to the host and port
        udp_socket.bind((host, udp_port))
        
        print(f"UDP server listening on port {udp_port}...")
        
        return udp_socket
    
    except OSError as e:
        print(f"Error binding UDP socket: {e}")
        sys.exit(1)



def loadfile(filename):
    #list of dictionary timetable to return storing depart time, route name, depart from, arrival time, arrival station
    timetable=[]
    with open (filename) as fread:

        # header of timetables, can remove if not needed. if removed modify earliest function to include the very first entry
        for line in fread:
            if not line.startswith("#"):
                stationInfo= line.strip().split(',')
                timetable.append({
                    'StationName': stationInfo[0],
                    'Longitude': stationInfo[1],
                    'Latitude': stationInfo[2],
                })
                break

        # actual timetable data
        for line in fread:
            if not line.startswith("#") and line.strip():
                datarow = line.strip().split(',')
                timetable.append({
                    'departTime':datarow[0],
                    'routeName': datarow[1],
                    'departFrom': datarow[2],
                    'arriveTime': datarow[3],
                    'arriveAt': datarow[4]
                })
        return timetable

#check happens in main. if file has been modified. reload the file by rereading the function
def update(filename):    
    return loadfile(filename)

def earliest(curTime, timetable):
    
    unique_destinations = {}

    #The for loop will do the following
    #filter the times that are not the past
    #if the destination is not in the unique_destinations entry add it
    #otherwise check the uniq_destination entry and update if required (keeping earliest time)
    for entry in timetable[1:]:
        destination = entry['arriveAt']
        depart_time = entry['departTime']
        depart_time_obj = datetime.datetime.strptime(depart_time, '%H:%M').time()   #convert text time into datetime time
        if depart_time_obj >= curTime:     
            if destination not in unique_destinations:
                unique_destinations[destination] = entry
            else:
                if depart_time < unique_destinations[destination]['departTime']:
                    unique_destinations[destination] = entry
                                                              

    return list(unique_destinations.values()) #convert back to list




 # ------------------ timetable functions end ---------------------------------




def main():
    if len(sys.argv) < 4:
        print("Usage: ./station <station_name> <tcp_port> <udp_port> [<neighbor1> <neighbor2> ...]")
        return
    
    try:
        # Extract the port numbers and optional neighbors from command-line arguments
        station_name = sys.argv[1]
        tcp_port = int(sys.argv[2])
        udp_port = int(sys.argv[3])
        neighbors = sys.argv[4:]
    except ValueError:
        print("Invalid port number")
        return

    # Start TCP server
    tcp_socket = tcp_server(tcp_port)
    
    # Start UDP server
    udp_socket = udp_server(udp_port, neighbors)

    # Create lists of sockets to monitor
    inputs = [tcp_socket, udp_socket]

    timetable = [] #full timetable stored. on startup empty
    filename = "tt-"+station_name
    modT= os.path.getmtime(filename)
    path=''
    while True:
        # Use select to wait for I/O events
        readable, _, _ = select.select(inputs, [], [])


        
        for sock in readable:
            if sock == tcp_socket:
                # Handle TCP connection
                client_socket, client_address = tcp_socket.accept()
                print(f"TCP connection established with {client_address}")

                # Receive data from the client
                data = client_socket.recv(1024)
                if data:
                    # Extract the value of 'to' parameter from the request
                    request = data.decode('utf-8')
                    match = re.search(r'GET /.*\?to=([^\s&]+)', request)
                    if match:
                        busport = match.group(1)
                        print("Selected busport:", busport)


                        # get current time (only applies in tcp connection. in udp it uses the arrival time in the string)
                        currentTime = datetime.datetime.now().time()
                        

                        if not timetable or modT != os.path.getmtime(filename) : 
                            modT=os.path.getmtime(filename)
                            print(modT)

                            timetable = loadfile(filename)
                            print("Timetable Updated\n")
                        
                        paths = earliest(currentTime, timetable)

                        # print("Full timetable")
                        # for item in timetable:print(item)
                        
                        print("Earliest neighbouring path")
                        for item in paths:print (item)



                        path += busport + ';' + str(udp_port)
                        
                        # Send busport to specified neighbors
                        if neighbors:
                            for neighbor in neighbors:
                                neighbor_host, neighbor_port = neighbor.split(':')
                                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                neighbor_socket.sendto(path.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                                neighbor_socket.close()

                # client_socket.close()
                
            elif sock == udp_socket:
                # Handle UDP message
                data, client_address = udp_socket.recvfrom(1024)
                print(f"Received UDP message from {client_address}: {data.decode('utf-8')}")
                # print("path" + path)
                # path += ';' + str(udp_port) 
                path = data.decode('utf-8') + ';' + str(udp_port)
                # print(data.decode('utf-8'))
                print(data.decode('utf-8'))
                print(path)

                # Optionally process data or communicate with neighbors
                if neighbors:
                    busport, source_port = data.decode('utf-8').split(';')
                    for neighbor in neighbors:
                        neighbor_host, neighbor_port = neighbor.split(':')
                        if neighbor_port != source_port:
                            neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                            neighbor_socket.sendto(data, (neighbor_host, int(neighbor_port)))
                            neighbor_socket.close()

if __name__ == "__main__":
    main()
