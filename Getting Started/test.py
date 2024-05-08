def main():
    paths = []
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
    payloadpath=''

    if not timetable or modT != os.path.getmtime(filename) : 
        modT=os.path.getmtime(filename)
        print(modT)

        timetable = loadfile(filename)
        print("Timetable Updated\n")

    while True:
        # Use select to wait for I/O events
        readable, _, _ = select.select(inputs, [], [])

        currentTime = datetime.datetime.now().time()  
        curTime = currentTime.replace(hour=12, minute=0, second=0, microsecond=0)  # Set the time to 12 PM
      



        paths = earliest(curTime, timetable)
        for item in paths:
            print(item)
        
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

                        # print("Full timetable")
                        # for item in timetable:print(item)
                        
                        print("Earliest neighbouring path")
                        for item in paths:print (item)


                    
                        payloadpath += busport + ';' + str(udp_port)
                        #path += 
                        # Send busport to specified neighbors
                        if neighbors:
                            path_udp = payloadpath.split(';')[1:]
                            for neighbor in neighbors:
                                neighbor_host, neighbor_port = neighbor.split(':')
                                if neighbor_port not in path_udp:
                                    #if clause --> 
                                    #path2 = path + ';' + paths[neighbor_port]['departTime'] + ";" + paths[neighbor_port]['arriveTime']      
                                    neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                    neighbor_socket.sendto(payloadpath.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                                    neighbor_socket.close()

                # client_socket.close()
                
            elif sock == udp_socket:
                # Handle UDP message
                data, client_address = udp_socket.recvfrom(1024)
                print(f"Received UDP message from {client_address}: {data.decode('utf-8')}")
                print("Appending my udp to the string.")
                payloadpath = data.decode('utf-8') + ';' + str(udp_port) + ';' + paths[0]['departTime'] + ";" + paths[0]['arriveTime']
                # print(data.decode('utf-8'))
                # print(data.decode('utf-8'))
                print(payloadpath)

                if neighbors:
                    path_udp = payloadpath.split(';')[1:]
                    for neighbor in neighbors:
                        neighbor_host, neighbor_port = neighbor.split(':')
                        if neighbor_port not in path_udp:
                            neighbor_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                            neighbor_socket.sendto(payloadpath.encode('utf-8'), (neighbor_host, int(neighbor_port)))
                            time.sleep(1)
                            neighbor_socket.close()

if __name__ == "__main__":
    main()