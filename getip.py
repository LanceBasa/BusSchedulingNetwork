import socket

def get_local_ip():
    try:
        # Create a socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            # Connect to an external server
            s.connect(("8.8.8.8", 80))
            local_ip = s.getsockname()[0]
        finally:
            # Ensure the socket is closed
            s.close()
    except Exception as e:
        print(f"An error occurred: {e}")
        local_ip = "Unable to get IP address"
    
    return local_ip

