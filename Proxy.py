import socket
import sys
import threading
import os
import time
import hashlib
from urllib.parse import urlparse

CACHE_DIR = 'cache'

def handle_client(client_socket):
    try:
        request = client_socket.recv(4096)
        if not request:
            client_socket.close()
            return

        headers_end = request.find(b'\r\n\r\n')
        if headers_end == -1:
            client_socket.sendall(b'HTTP/1.1 400 Bad Request\r\n\r\n')
            client_socket.close()
            return

        headers_part = request[:headers_end]
        headers = headers_part.decode().split('\r\n')
        first_line = headers[0].split()
        if len(first_line) < 3:
            client_socket.sendall(b'HTTP/1.1 400 Bad Request\r\n\r\n')
            client_socket.close()
            return

        method, url, version = first_line
        if method.upper() != 'GET':
            client_socket.sendall(b'HTTP/1.1 501 Not Implemented\r\n\r\n')
            client_socket.close()
            return

        parsed_url = urlparse(url)
        host = parsed_url.hostname
        if not host:
            client_socket.sendall(b'HTTP/1.1 400 Bad Request\r\n\r\n')
            client_socket.close()
            return

        port = parsed_url.port if parsed_url.port else 80
        path = parsed_url.path if parsed_url.path else '/'
        if parsed_url.query:
            path += '?' + parsed_url.query

        cache_key = hashlib.md5(url.encode()).hexdigest()
        cache_path = os.path.join(CACHE_DIR, cache_key)
        cached = False

        if os.path.exists(cache_path):
            with open(cache_path, 'rb') as f:
                content = f.read()
                exp_end = content.find(b'\n')
                if exp_end != -1:
                    exp_str = content[:exp_end].decode()
                    try:
                        exp_time = float(exp_str)
                        if time.time() < exp_time:
                            cached_response = content[exp_end+1:]
                            client_socket.sendall(cached_response)
                            cached = True
                        else:
                            os.remove(cache_path)
                    except ValueError:
                        pass

        if not cached:
            try:
                origin_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                origin_socket.connect((host, port))
                request_headers = [
                    f'GET {path} HTTP/1.1',
                    f'Host: {host}:{port}' if port != 80 else f'Host: {host}',
                    'Connection: close',
                    '\r\n'
                ]
                request_msg = '\r\n'.join(request_headers)
                origin_socket.sendall(request_msg.encode())

                response = b''
                while True:
                    data = origin_socket.recv(4096)
                    if not data:
                        break
                    response += data

                origin_socket.close()

                headers_end_idx = response.find(b'\r\n\r\n')
                if headers_end_idx == -1:
                    client_socket.sendall(b'HTTP/1.1 502 Bad Gateway\r\n\r\n')
                    client_socket.close()
                    return

                headers_part = response[:headers_end_idx + 4]
                headers = headers_part.decode().split('\r\n')
                cache_control = None
                for header in headers:
                    if header.lower().startswith('cache-control'):
                        cache_control = header.split(':', 1)[1].strip()
                        break

                max_age = None
                if cache_control:
                    for part in cache_control.split(','):
                        part = part.strip()
                        if part.startswith('max-age='):
                            try:
                                max_age = int(part.split('=', 1)[1])
                            except ValueError:
                                pass
                            break

                if max_age is not None and max_age > 0:
                    expiration = time.time() + max_age
                    os.makedirs(CACHE_DIR, exist_ok=True)
                    with open(cache_path, 'wb') as f:
                        f.write(f'{expiration}\n'.encode())
                        f.write(response)

                client_socket.sendall(response)
            except Exception as e:
                client_socket.sendall(b'HTTP/1.1 502 Bad Gateway\r\n\r\n')
    except Exception as e:
        pass
    finally:
        client_socket.close()

def main():
    if len(sys.argv) != 3:
        print("Usage: python Proxy.py <host> <port>")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])

    os.makedirs(CACHE_DIR, exist_ok=True)

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((host, port))
    server_socket.listen(5)
    print(f"Proxy server listening on {host}:{port}")

    try:
        while True:
            client_socket, addr = server_socket.accept()
            print(f"Accepted connection from {addr[0]}:{addr[1]}")
            client_handler = threading.Thread(target=handle_client, args=(client_socket,))
            client_handler.start()
    except KeyboardInterrupt:
        print("Shutting down proxy server")
        server_socket.close()

if __name__ == "__main__":
    main()