import socket

def send_udp_message(message, host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(message.encode(), (host, port))
        print(f"Message sent to {host}:{port}")
    finally:
        sock.close()

    print("Done sending")

# Example usage
if __name__ == '__main__':
    send_udp_message("Hello UDP Server", 'localhost', 4004)
