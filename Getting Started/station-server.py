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
    tcp_socket = tcp_server(tcp_port)
    
    # Start UDP server
    udp_socket = udp_server(udp_port, neighbors)

    # Create lists of sockets to monitor
    inputs = [tcp_socket, udp_socket]

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

                        # Append source UDP port to busport with ';' as delimiter
                        busport += ';' + str(udp_port)
                        
                        # Send busport to specified neighbors
                        if neighbors:
                            for neighbor in neighbors:
                                neighbor_host, neighbor_port = neighbor.split(':')
                                neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                neighbor_socket.sendto(busport.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                                neighbor_socket.close()

                # client_socket.close()
                
            elif sock == udp_socket:
                # Handle UDP message
                data, client_address = udp_socket.recvfrom(1024)
                print(f"Received UDP message from {client_address}: {data.decode('utf-8')}")

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
