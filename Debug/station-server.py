# Title: Computer Networks Project
# Authors: Midhin Viju(22850881), Sahil Narula (23313963), William Salim (23291312), Lance Basa (23420659)
# References
# A Guide to Network Programming using Internet sockets, by Brian "Beej" Hall. 
# ChatGPT
# https://www.geeksforgeeks.org/socket-programming-cc/
# Transports and Protocols, socket - Low-level networking interface, and socketserver - A framework for network servers, all from python.org.

import socket
import sys
import re
import select
import os
import datetime
import time
import urllib.parse

# Get the number of neighbors from command line arguments
num_neighbors = len(sys.argv) - 4
print(num_neighbors)
neighbor_dictionary = {}

def get_local_ip():
    """Retrieve the local IP address of the machine."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
        finally:
            s.close()
            return local_ip
    except Exception as e:
        print(f"An error occurred: {e}")
        return None

def tcp_server(myIP, tcp_port):
    """Start a TCP server."""
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        tcp_socket.bind((myIP, tcp_port))
        tcp_socket.listen(5)
        print(f"TCP server listening for incoming connections on port {tcp_port}...")
        return tcp_socket
    except OSError as e:
        print(f"Error binding TCP socket: {e}")
        sys.exit(1)

def udp_server(myIP, udp_port):
    """Start a UDP server."""
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        udp_socket.bind((myIP, udp_port))
        print(f"UDP server listening on port {udp_port}...")
        return udp_socket
    except OSError as e:
        print(f"Error binding UDP socket: {e}")
        sys.exit(1)

def loadfile(filename):
    """Load the timetable from a file."""
    timetable = []
    with open(filename) as fread:
        for line in fread:
            if not line.startswith("#"):
                stationInfo = line.strip().split(',')
                timetable.append({
                    'StationName': stationInfo[0],
                    'Longitude': stationInfo[1],
                    'Latitude': stationInfo[2],
                })
                break
        for line in fread:
            if not line.startswith("#") and line.strip():
                datarow = line.strip().split(',')
                timetable.append({
                    'departTime': datarow[0],
                    'busNumber': datarow[1],
                    'departFrom': datarow[2],
                    'arriveTime': datarow[3],
                    'arriveAt': datarow[4]
                })
    return timetable

def update(filename):
    """Update the timetable by reloading the file."""
    return loadfile(filename)

def earliest(curTime, timetable):
    """Get the earliest departure times for each destination."""
    unique_destinations = {}
    for entry in timetable[1:]:
        destination = entry['arriveAt']
        depart_time = entry['departTime']
        depart_time_obj = datetime.datetime.strptime(depart_time, '%H:%M').time()
        if depart_time_obj >= curTime:
            if destination not in unique_destinations:
                unique_destinations[destination] = entry
            else:
                if depart_time < unique_destinations[destination]['departTime']:
                    unique_destinations[destination] = entry
    return list(unique_destinations.values())

def update_timetable(tt, filename, modT):
    """Update the timetable if the file has been modified."""
    if not tt or modT != os.path.getmtime(filename):
        tt = loadfile(filename)
        print("Timetable Updated\n")
        return tt
    return tt

def get_ttEntry(stationName, timetable_list):
    """Get timetable entry for a specific station."""
    for index, item in enumerate(timetable_list):
        if item.get('arriveAt') == stationName:
            return index
    return None

def search_by_value(dictionary, search_value):
    """Search for a value in a dictionary and return the corresponding key."""
    for key, value in dictionary.items():
        if value[1] == search_value:
            return key
    return None

def backtrack(path):
    """Send the path back to the previous station."""
    split_path = path.split('-')[:-1]
    backtrackStr = "-".join(split_path)
    path_len = len(split_path)
    if path_len > 1:
        previous_station = split_path[-1]
        key = search_by_value(neighbor_dictionary, previous_station)
        previous_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        previous_socket.sendto(backtrackStr.encode('utf-8'), (neighbor_dictionary[key][0], int(key)))
        previous_socket.close()

def ping_neighbours(neighbors, station_name, udp_port, udp_socket,local_IP):
    """Ping neighbors to get their details."""
    neighbor_hosts = {}
    for neighbor in neighbors:
        neighbor_host, neighbor_port = neighbor.split(':')
        neighbor_hosts[neighbor_port] = neighbor_host

    for i in range(10):

        if neighbors:
            for neighbor_port, neighbor_host in neighbor_hosts.items():
                if neighbor_host == "localhost":
                    neighbor_host=local_IP

                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                send_info = "!" + station_name + ":" + str(udp_port)
                neighbor_socket.sendto(send_info.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                neighbor_socket.close()

            data, client_address = udp_socket.recvfrom(1024)
            if data:
                sName, portNum = data.decode('utf-8')[1:].split(':')
                print(f"Received UDP message from {client_address}: {sName}:{portNum}")

                if portNum not in neighbor_dictionary.keys() and portNum in neighbor_hosts:
                    if neighbor_hosts[portNum] == "localhost":
                        neighbor_hosts[portNum]=local_IP

                    

                    neighbor_dictionary[portNum] = [neighbor_hosts[portNum], sName]
            time.sleep(1)
    return neighbor_dictionary

def getStations(string):
    """Extract stations from the given path string."""
    parts = string.split(';')
    del parts[0]
    stations_list = [parts[i] for i in range(0, len(parts), 4)]
    return stations_list

def timeout_handler(signum, frame, tcp_socket):
    """Handle timeout during TCP processing."""
    print("Timeout occurred!")
    with open('response_page.html', 'r') as file:
        client_socket, client_address = tcp_socket.accept()
        response_template = file.read()
        response_html = response_template.replace('{{Here_is_your_route}}', 'Timed out, no path found')
        response = f"HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: {len(response_html)}\r\n\r\n{response_html}"
        client_socket.sendall(response.encode('utf-8'))
        client_socket.close()

def main():
    if len(sys.argv) < 4:
        print("Usage: ./station-server.py <station_name> <tcp_port> <udp_port> [<neighbor1> <neighbor2> ...]")
        return

    try:
        station_name = sys.argv[1]
        tcp_port = int(sys.argv[2])
        udp_port = int(sys.argv[3])
        neighbors = sys.argv[4:]
    except ValueError:
        print("Invalid port number")
        return

    # Start TCP and UDP servers
    local_IP = get_local_ip()
    print(local_IP)
    tcp_socket = tcp_server(local_IP, tcp_port)
    udp_socket = udp_server(local_IP, udp_port)

    inputs = [tcp_socket, udp_socket]
    filename = "tt-" + station_name
    timetable = loadfile(filename)
    modT = os.path.getmtime(filename)
    path = ''

    time.sleep(2)
    neighbor_dictionary = ping_neighbours(neighbors, station_name, udp_port, udp_socket,local_IP)

    os.system('clear')

    print("Timetable Loaded")
    print(f"I am {station_name} my neighbors are {neighbor_dictionary}")

    while True:
        readable, _, _ = select.select(inputs, [], [])

        for sock in readable:
            if sock == tcp_socket:
                client_socket, client_address = tcp_socket.accept()
                print(f"TCP connection established with {client_address}")

                data = client_socket.recv(1024)
                if data:
                    request = data.decode('utf-8')
                    match = re.search(r'GET /.*\?departure-time=([^&]+)&to=([^\s&]+)', request)
                    if match:
                        departure_time_encoded = match.group(1)
                        departure_time = urllib.parse.unquote(departure_time_encoded)
                        busport = match.group(2).strip()
                        if station_name == busport:
                            print(f"You are already at {station_name}")
                            break
                        else:
                            print(f"Leaving {station_name} at {departure_time} to go to {busport}")
                        currentTime = datetime.datetime.strptime(departure_time, '%H:%M').time()

                        timetable = update_timetable(timetable, filename, modT)
                        earliestPaths = earliest(currentTime, timetable)
                        if not earliestPaths:
                            print("No more buses for today. Walk home ;(")
                            break

                        path_taken = ' '
                        if neighbor_dictionary:
                            for neighbor in neighbor_dictionary:
                                tt_index = ''
                                neighbor_host = neighbor_dictionary[neighbor][0]
                                neighbor_name = neighbor_dictionary[neighbor][1]

                                if earliestPaths:
                                    tt_index = get_ttEntry(neighbor_name, earliestPaths)
                                    if tt_index is None:
                                        continue
                                    earliestRide = earliestPaths[tt_index]
                                    pathUpdate = busport + ';' + station_name + ';' + earliestRide['busNumber'] + ';' + earliestRide['departTime'] + ';' + earliestRide['arriveTime']
                                else:
                                    print(f"No more buses/trains leaving at this hour {currentTime}")

                                if neighbor_name not in path_taken:
                                    neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                    neighbor_socket.sendto(pathUpdate.encode('utf-8'), (neighbor_host, int(neighbor)))
                                    neighbor_socket.close()
            elif sock == udp_socket:
                data, client_address = udp_socket.recvfrom(1024)
                dataIdentifyer = data.decode('utf-8')[0]

                if dataIdentifyer != '!' and dataIdentifyer != '~':
                    path = data.decode('utf-8')
                    print(f"\nReceived UDP message from {client_address}: {path}")
                    currentTime = datetime.datetime.strptime(path.split(';')[-1], '%H:%M').time()

                    final_destination = path.split(';')[0]
                    if station_name == final_destination:
                        print(f"\nQuery Success! Current time: {currentTime}\nPath taken: {path}\nNow returning query to source!\n")
                        path += ';' + station_name
                        result = getStations(path)[:]
                        path = "~" + path
                        for item in result:
                            path += "-" + item
                        backtrack(path)
                        break

                    timetable = update_timetable(timetable, filename, modT)
                    earliestPaths = earliest(currentTime, timetable)
                    if not earliestPaths:
                        print("No more buses for today. Walk home ;(")
                        break

                    path_taken = path.split(';')[1:]
                    if neighbor_dictionary:
                        for neighbor in neighbor_dictionary:
                            tt_index = ''
                            neighbor_host = neighbor_dictionary[neighbor][0]
                            neighbor_name = neighbor_dictionary[neighbor][1]

                            if earliestPaths:
                                tt_index = get_ttEntry(neighbor_name, earliestPaths)
                                if tt_index is None:
                                    continue
                                earliestRide = earliestPaths[tt_index]
                                pathUpdate = path + ';' + station_name + ';' + earliestRide['busNumber'] + ';' + earliestRide['departTime'] + ';' + earliestRide['arriveTime']
                            else:
                                print(f"No more buses/trains leaving at this hour {currentTime}")

                            if neighbor_name not in path_taken:
                                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                neighbor_socket.sendto(pathUpdate.encode('utf-8'), (neighbor_host, int(neighbor)))
                                neighbor_socket.close()
                elif dataIdentifyer == '~':
                    backPath = data.decode('utf-8')
                    result = backPath.split(";")
                    backtrack_1 = result[-1].split("-")
                    result[-1] = backtrack_1[0]

                    output_to_html = ''
                    if station_name == result[1]:
                        print(f"\nFrom {station_name} going to {result[-1]}")
                        for item in range(1, len(result) - 3, 4):
                            output_to_html += f"\tFrom {result[item]} catch {result[item+1]} leaving at {result[item+2]} and arrived at {result[item+4]} at {result[item+3]}\n"
                        print(output_to_html)
                        time.sleep(2)
                        output_to_html = "<html><body>" + '<h1>Result</h1> ' + output_to_html + " <a href=\"mywebpage.html\">Go back</a></body></html>"
                        response = f"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n{output_to_html}\r\n\r\n"
                        client_socket.sendall(response.encode('utf-8'))
                    else:
                        print(f"\nReceived backtrack message {backPath}")
                        backtrack(backPath)

if __name__ == "__main__":
    main()
