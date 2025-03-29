# Proxy-bonus.py
# Bonus Features Implemented:
# 1. Handle Expires header in addition to Cache-Control (2 pts)
# 2. Pre-fetch linked resources in HTML (2 pts)
# 3. Support origin URLs with custom ports, e.g. hostname:port/file (2 pts)

import socket
import sys
import os
import re
import time
import hashlib
from urllib.parse import urlparse
from html.parser import HTMLParser
import threading

BUFFER_SIZE = 1000000
CACHE_ROOT = './cache_bonus'

class LinkParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links = []

    def handle_starttag(self, tag, attrs):
        for attr, val in attrs:
            if attr in ('src', 'href'):
                self.links.append(val)

def parse_headers(raw):
    headers = {}
    lines = raw.split('\r\n')
    for line in lines:
        if ':' in line:
            k, v = line.split(':', 1)
            headers[k.strip().lower()] = v.strip()
    return headers

def get_cache_path(host, port, path):
    identifier = f'{host}:{port}{path}'
    hashname = hashlib.md5(identifier.encode()).hexdigest()
    return os.path.join(CACHE_ROOT, hashname)

def prefetch_resources(html_content, base_host, port):
    parser = LinkParser()
    try:
        parser.feed(html_content)
    except:
        return
    for link in parser.links:
        parsed = urlparse(link)
        if parsed.scheme and parsed.netloc:
            host = parsed.hostname
            path = parsed.path
        else:
            host = base_host
            path = parsed.path if parsed.path else '/'
        threading.Thread(target=fetch_and_cache, args=(host, port, path)).start()

def fetch_and_cache(host, port, path):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        req = f'GET {path} HTTP/1.1\r\nHost: {host}\r\nConnection: close\r\n\r\n'
        s.sendall(req.encode())
        response = b''
        while True:
            data = s.recv(BUFFER_SIZE)
            if not data:
                break
            response += data
        s.close()
        cache_path = get_cache_path(host, port, path)
        headers_end = response.find(b'\r\n\r\n')
        meta_line = b''
        headers = {}
        if headers_end != -1:
            header_text = response[:headers_end].decode(errors='ignore')
            headers = parse_headers(header_text)
            if 'cache-control' in headers:
                for part in headers['cache-control'].split(','):
                    if 'max-age=' in part:
                        try:
                            max_age = int(part.strip().split('=')[1])
                            exp = time.time() + max_age
                            meta_line = f"{exp}\n".encode()
                        except: pass
            elif 'expires' in headers:
                try:
                    exp_time = time.mktime(time.strptime(headers['expires'], '%a, %d %b %Y %H:%M:%S %Z'))
                    meta_line = f"{exp_time}\n".encode()
                except: pass
        os.makedirs(CACHE_ROOT, exist_ok=True)
        with open(cache_path, 'wb') as f:
            f.write(meta_line + response)
    except:
        pass

def main():
    if len(sys.argv) != 3:
        print("Usage: python Proxy-bonus.py <host> <port>")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2])
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(5)
    print(f"[+] Listening on {host}:{port}")

    while True:
        client, addr = server.accept()
        print(f"[+] Connection from {addr}")
        req = client.recv(BUFFER_SIZE)
        if not req:
            client.close()
            continue
        try:
            first_line = req.decode(errors='ignore').split('\r\n')[0]
            method, url, _ = first_line.split()
            if method != 'GET':
                client.sendall(b'HTTP/1.1 501 Not Implemented\r\n\r\n')
                client.close()
                continue
            parsed = urlparse(url)
            h = parsed.hostname
            p = parsed.port if parsed.port else 80
            path = parsed.path if parsed.path else '/'
            if parsed.query:
                path += '?' + parsed.query
            cache_path = get_cache_path(h, p, path)
            cached = False
            if os.path.exists(cache_path):
                with open(cache_path, 'rb') as f:
                    meta = f.readline()
                    try:
                        exp = float(meta.decode())
                        if time.time() < exp:
                            client.sendall(f.read())
                            cached = True
                    except: pass
            if not cached:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.connect((h, p))
                proxy_req = f"GET {path} HTTP/1.1\r\nHost: {h}\r\nConnection: close\r\n\r\n"
                s.sendall(proxy_req.encode())
                response = b''
                while True:
                    data = s.recv(BUFFER_SIZE)
                    if not data:
                        break
                    response += data
                s.close()
                client.sendall(response)
                threading.Thread(target=prefetch_resources, args=(response.decode(errors='ignore'), h, p)).start()
                fetch_and_cache(h, p, path)
        except:
            client.sendall(b'HTTP/1.1 500 Internal Error\r\n\r\n')
        client.close()

if __name__ == '__main__':
    main()
