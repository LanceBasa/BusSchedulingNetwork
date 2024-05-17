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
import time
import signal



import urllib.parse



num_neighbors = len(sys.argv) - 4
print( num_neighbors)
neighbor_dictionary={}

def tcp_server(tcp_port):
    # Define the host
    host = '10.135.123.132'  # Listen on localhost
    
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
    host = '10.135.123.132'  # Listen on localhost
    
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
                    'busNumber': datarow[1],
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


def update_timetable(tt, filename, modT):
    if not tt or modT != os.path.getmtime(filename) : 
        tt = loadfile(filename)
        print("Timetable Updated\n")
        return tt
    return tt

def get_ttEntry(stationName,timetable_list):
    for index, item in enumerate(timetable_list):
        if item.get('arriveAt') == stationName:
            # If 'JunctionE' is found, print its index position
            return index
    return None

 # ------------------ timetable functions end ---------------------------------
def search_by_value(dictionary, search_value):
    # Iterate through the dictionary's values
    for key, value in dictionary.items():
        if value[1] == search_value:
            return key  # Return the key corresponding to the search value
    return None  # Return None if the value is not found

def backtrack(path):
    split_path = path.split('-')
    split_path= split_path[:-1]
    backtrackStr = "-".join(split_path)
    path_len = len(split_path)
    if path_len >1:
        previous_station = split_path[-1]

        key = search_by_value(neighbor_dictionary, previous_station)
        previous_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  
        previous_socket.sendto(backtrackStr.encode('utf-8'), (neighbor_dictionary[key][0], int(key)))
        previous_socket.close()

def ping_neighbours(neighbors, station_name, udp_port, udp_socket):
    neighbor_hosts = {}  # Dictionary to store neighbor hosts by their ports
    for neighbor in neighbors:
        neighbor_host, neighbor_port = neighbor.split(':')
        neighbor_hosts[neighbor_port] = neighbor_host  # Store the neighbor's host by their port

    for i in range(10):
        if neighbors:
            for neighbor_port, neighbor_host in neighbor_hosts.items():
                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                send_info = "!" + station_name + ":" + str(udp_port)
                neighbor_socket.sendto(send_info.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                neighbor_socket.close()

            data, client_address = udp_socket.recvfrom(1024)
            if data:
                sName, portNum = data.decode('utf-8')[1:].split(':')
                print(f"Received UDP message from {client_address}: {sName}:{portNum}")

                # Use the neighbor_host from the dictionary based on the portNum
                if portNum not in neighbor_dictionary.keys() and portNum in neighbor_hosts:
                    neighbor_dictionary[portNum] = [neighbor_hosts[portNum], sName]
            time.sleep(1)

    return neighbor_dictionary

        


def getStations(string):
    parts = string.split(';')
    del parts[0]
    stations_list = [parts[i] for i in range(0, len(parts), 4)]
    return stations_list

# Global flag to track timeout occurrence

def get_tcp_socket(tcp_socket):
    return tcp_socket

def timeout_handler(signum, frame,tcp_socket):
    timeout_occurred = True
    print("Timeout occurred!")
    if timeout_occurred:
        print("Timeout occurred during TCP processing.")
            # Handle timeout...   
        with open('response_page.html', 'r') as file:
                    
                    client_socket, client_address = tcp_socket.accept()

                    response_template = file.read()

                    # Replace the placeholder in the HTML template with the dynamic content
                    response_html = response_template.replace('{{Here_is_your_route}}', 'Timed out no path found')

                    # Construct the HTTP response with the complete HTML content
                    response = f"HTTP/1.1 404 Not found\r\nContent-Type: text/html\r\nContent-Length: {len(response_html)}\r\n\r\n{response_html}"
                    client_socket.sendall(response.encode('utf-8'))
                    client_socket.close()




def main():
    result = []
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
    filename = "tt-"+station_name

    timetable = loadfile(filename)

    modT= os.path.getmtime(filename)
    path=''

    time.sleep(2)
    neighbor_dictionary = ping_neighbours(neighbors,station_name,udp_port,udp_socket)

    os.system('clear') #NOTE REMOVE LATER

    print("Timetable Loaded")
    print(f"I am {station_name} my neighbours are{neighbor_dictionary}")


    while True:

       


        # Use select to wait for I/O events
        readable, _, _ = select.select(inputs, [], [])


        
        for sock in readable:
            if sock == tcp_socket:

                
                
                # Handle TCP connection
                client_socket, client_address = tcp_socket.accept()
                print(f"TCP connection established with {client_address}")



                # Start the global timer when a TCP request is received
                # start_global_timer(4, tcp_socket)  # 10 seconds timeout
                # Receive data from the client
                data = client_socket.recv(1024)
                print (data)
                if data:
                    # Extract the value of 'to' parameter from the request
                        request = data.decode('utf-8')
                        match = re.search(r'GET /.*\?departure-time=([^&]+)&to=([^\s&]+)', request)
                        if match:
                            departure_time_encoded = match.group(1)  # Extract departure time
                            departure_time = urllib.parse.unquote(departure_time_encoded)  # Decode URL encoding
                            busport = match.group(2).strip() 
                            if station_name== busport:
                                print(f"You are already at {station_name}")
                                break
                            else:
                                print(f"Leaving {station_name} at {departure_time} to go to {busport}")
                        currentTime = datetime.datetime.strptime(departure_time, '%H:%M').time()


                        
                    # request = data.decode('utf-8')
                    # match = re.search(r'GET /.*\?to=([^\s&]+)', request)
                    # if match:
                    #     print("hehe")
                    #     busport = match.group(1)
                    #     print("Selected busport:", busport)


                    #     # get current time (only applies in tcp connection. in udp it uses the arrival time in the string)
                    #     currentTime = datetime.datetime.now().time()


                        # check timetable for change every tcp and udp request
                        timetable=update_timetable(timetable, filename, modT)

                        # get current time (only applies in tcp connection. in udp it uses the arrival time in the string)
                        earliestPaths = earliest(currentTime, timetable)
                        if not earliestPaths: print("No more bus for today. Walk home ;("); break

                        

                        

                        # path += busport + ';' + station_name
                        path_taken =' '
                        # Send string to neighbours
                        if neighbor_dictionary:
                            for neighbor in neighbor_dictionary:
                                tt_index=''
                                neighbor_host=neighbor_dictionary[neighbor][0]
                                neighbor_name=neighbor_dictionary[neighbor][1]

                                if earliestPaths:

                                    tt_index = get_ttEntry(neighbor_name,earliestPaths)
                                    if tt_index is None: continue
                                    earliestRide = earliestPaths[tt_index]
                                    pathUpdate = busport + ';' + station_name +';'+ earliestRide['busNumber']+';' + earliestRide['departTime']+ ';'+ earliestRide['arriveTime']
                                else: print(f"no more bus/train leaving at this hour{currentTime}")

                                if neighbor_name not in path_taken:
                                    neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                    neighbor_socket.sendto(pathUpdate.encode('utf-8'), (neighbor_host, int(neighbor)))
                                    neighbor_socket.close()
                # client_socket.close()
                # client_socket.close()
                
            elif sock == udp_socket:
                
                
                                # Handle UDP message
                data, client_address = udp_socket.recvfrom(1024)
                dataIdentifyer =  data.decode('utf-8')[0]

                # If statement to check what is being recieved. ! is a ping and ~ is returning data.
                if dataIdentifyer != '!' and dataIdentifyer != '~':
                    path = data.decode('utf-8')
                    print(f"\nReceived UDP message from {client_address}: {path}")
                    currentTime = datetime.datetime.strptime(path.split(';')[-1], '%H:%M').time()

                    # Before continue and sending to other neighbours, check if the current station is the destination. 
                    # If so, send response back to the source and break
                    final_destination=path.split(';')[0]
                    if station_name == final_destination:
                        print(f"\nQuery Success!\tCurrent time:{currentTime}\nPath taken:{path}\nNow returning query to source!\n")
                        path += ';'+ station_name
                        result = getStations(path)[:]
                        path = "~" + path  
                        for item in result:
                            path += "-" + item
                        backtrack(path)
                        break

                    # check timetable for change every tcp and udp request
                    timetable=update_timetable(timetable, filename, modT)

                    # get current time - this is arrival time of previous station. different to tcp
                    earliestPaths = earliest(currentTime, timetable)
                    if not earliestPaths: print("No more bus for today. Walk home ;("); break

                    
                    #if this station is not the final destination then send udp to neighbours.
                    path_taken = path.split(';')[1:]
                    if neighbor_dictionary:
                        for neighbor in neighbor_dictionary:
                            tt_index=''
                            neighbor_host=neighbor_dictionary[neighbor][0]
                            neighbor_name=neighbor_dictionary[neighbor][1]
                            
                            if earliestPaths:
                                tt_index = get_ttEntry(neighbor_name,earliestPaths)
                                if tt_index is None: continue
                                earliestRide = earliestPaths[tt_index]
                                pathUpdate = path + ';' + station_name +';'+ earliestRide['busNumber']+ ';' + earliestRide['departTime']+ ';'+ earliestRide['arriveTime'] 
                            else: print(f"no more bus/train leaving at this hour{currentTime}")

                            if neighbor_name not in path_taken:         # to avoid looping, only send to neighbours that have not been visited.
                                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                neighbor_socket.sendto(pathUpdate.encode('utf-8'), (neighbor_host, int(neighbor)))
                                neighbor_socket.close()
                    # client_socket.close()
                


                # This is to identify the incoming data as returning data
                elif dataIdentifyer == '~':
                    backPath = data.decode('utf-8')

                    
                    result = backPath.split(";") #
                    backtrack_1 = result[-1].split("-")
                    result[-1] = backtrack_1[0]

                    output_to_html =''

                    #if this is the source station name who got the request from client return string information to client.
                    if station_name == result[1]:
                        print(f"\nFrom {station_name} going to {result[-1]}")
                        

                        for item in range(1, len(result) - 3,4):
                            # print(f"You departed from {result[item]} at {result[item+1]} and arrived at {result[item+3]} at {result[item+2]}" )
                            # output_to_html += f"You departed from {result[item]} at {result[item+1]} and arrived at {result[item+3]} at {result[item+2]}\n" 
                            output_to_html += f"\tFrom {result[item]} catch {result[item+1]} leaving at {result[item+2]} and arrived at {result[item+4]} at {result[item+3]}\n" 
                        
                        print(output_to_html)
                        time.sleep(2)
                        output_to_html ="<html><body>"+'<h1>Result</h1> ' + output_to_html+ " <a href=\"mywebpage.html\">Go back</a></body></html>"

                        response = f"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n {output_to_html}\r\n\r\n"
                        client_socket.sendall(response.encode('utf-8'))
                        # client_socket.close()
                    else:
                        print(f"\nReceived backtrack message {backPath}")
                        backtrack(backPath)
# Handle timeouts
                                     
            
            


            

                        

                    

if __name__ == "__main__":
    main()


#commands to run servers to use the timetable in getting started directory
# python3 ./station-server.py BusportB 4003 4004 localhost:4006 localhost:4010 
# python3 ./station-server.py StationC 4005 4006 localhost:4004 localhost:4008 localhost:4010 
# python3 ./station-server.py JunctionE 4009 4010 localhost:4002 localhost:4004 localhost:4006 
# python3 ./station-server.py JunctionA 4001 4002 localhost:4010 localhost:4012 
# python3 ./station-server.py TerminalD 4007 4008 localhost:4006 
# python3 ./station-server.py BusportF 4011 4012 localhost:4002