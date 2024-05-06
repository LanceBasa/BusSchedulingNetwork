import socket
import urllib.parse



def parse_http_request(request_data):
    lines = request_data.split('\r\n')
    method, path, _ = lines[0].split(' ')
    headers = {}
    body = None

    # Parse headers
    for line in lines[1:]:
        if line.strip():
            key, value = line.split(':', 1)
            headers[key.strip()] = value.strip()

    # Parse body if present (POST request)
    if method == 'POST':
        body = lines[-1]

    return method, path, headers, body

def create_http_response(status_code, content_type, content):
    response = f"HTTP/1.1 {status_code}\r\n"
    response += f"Content-Type: {content_type}\r\n"
    response += f"Content-Length: {len(content)}\r\n"
    response += "\r\n"
    response += content
    return response

def start_server(port):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('localhost', port))
    server_socket.listen(5)
    print(f"Server is listening on port {port}")

    try:
        while True:
            client_socket, client_address = server_socket.accept()
            request_data = client_socket.recv(1024).decode('utf-8')

            if request_data:
                method, path, headers, body = parse_http_request(request_data)

                if method == 'GET' and path == '/':
                    content = "Hello, GET request received!"
                    response = create_http_response('200 OK', 'text/plain', content)
                elif method == 'POST' and path == '/':
                    content = f"Hello, POST request received! Body: {body}"
                    response = create_http_response('200 OK', 'text/plain', content)
                else:
                    content = "{'departTime': '10:03', 'routeName': 'busA_F', 'departFrom': 'stopA', 'arriveTime': '10:51', 'arriveAt': 'BusportF'}"
                    response = create_http_response('404 Not Found', 'text/plain', content)

                client_socket.sendall(response.encode('utf-8'))
                client_socket.close()
    except KeyboardInterrupt:
        print("\nServer stopped by user.")
    finally:
        server_socket.close()
        print("Server closed. Port released.")

if __name__ == "__main__":
    port_input = input("Enter the port number to start the server: ")
    try:
        port = int(port_input)
        start_server(port)
    except ValueError:
        print("Invalid port number. Please enter a valid integer port number.")
