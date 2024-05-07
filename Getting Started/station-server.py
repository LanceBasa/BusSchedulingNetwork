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

def tcp_server(tcp_port):
    # Define the host
    host = 'localhost'  # Listen on localhost
    
    # Create a socket object
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        # Bind the socket to the host and port
        server_socket.bind((host, tcp_port))
        
        # Start listening for incoming connections
        server_socket.listen(1)
        print(f"TCP server listening for incoming connections on port {tcp_port}...")
        
        # Accept incoming connections
        client_socket, client_address = server_socket.accept()
        print(f"TCP connection established with {client_address}")
        
        # Receive data from the client
        while True:
            data = client_socket.recv(1024)
            if not data:
                break
            
            # Extract the value of 'to' parameter from the request
            request = data.decode('utf-8')
            match = re.search(r'GET /.*\?to=([^\s&]+)', request)
            if match:
                busport = match.group(1)
                print("Selected busport:", busport)
            
    except KeyboardInterrupt:
        print("TCP server stopped by user.")
        server_socket.close()
        
    finally:
        # Close the socket
        server_socket.close()

def udp_server(udp_port, neighbors=None):
    # Define the host
    host = 'localhost'  # Listen on localhost
    
    # Create a socket object
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        # Bind the socket to the host and port
        server_socket.bind((host, udp_port))
        
        print(f"UDP server listening on port {udp_port}...")
        
        # Receive data from clients
        while True:
            data, client_address = server_socket.recvfrom(1024)
            if not data:
                break
            
            print(f"Received UDP message from {client_address}: {data.decode('utf-8')}")
            
            # Optionally process data or communicate with neighbors
            if neighbors:
                for neighbor in neighbors:
                    neighbor_host, neighbor_port = neighbor.split(':')
                    neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    neighbor_socket.sendto(data, (neighbor_host, int(neighbor_port)))
                    neighbor_socket.close()
            
    except KeyboardInterrupt:
        print("UDP server stopped by user.")
        server_socket.close()
        
    finally:
        # Close the socket
        server_socket.close()

def main():
    if len(sys.argv) < 3:
        print("Usage: ./station <tcp_port> <udp_port> [<neighbor1> <neighbor2> ...]")
        return
    
    try:
        # Extract the port numbers and optional neighbors from command-line arguments
        tcp_port = int(sys.argv[1])
        udp_port = int(sys.argv[2])
        neighbors = sys.argv[3:]
    except ValueError:
        print("Invalid port number")
        return

    # Start TCP server
    tcp_server(tcp_port)
    
    # Start UDP server
    udp_server(udp_port, neighbors)

if __name__ == "__main__":
    main()

