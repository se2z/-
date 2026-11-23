import socket
import time

def preflight():
    return True

def _read(sock, count):
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count - len(data))
        if not part:
            raise OSError("partial proxy handshake")
        data.extend(part)
    return bytes(data)

def http_connect(host, port, target, target_port, timeout):
    preflight()
    sock = socket.create_connection((host, port), timeout)
    started = time.monotonic()
    try:
        request = f"CONNECT {target}:{target_port} HTTP/1.1\r\nHost: {target}:{target_port}\r\n\r\n".encode("ascii")
        sock.sendall(request)
        response = bytearray()
        while b"\r\n\r\n" not in response:
            response.extend(_read(sock, 1))
            if len(response) > 16384:
                raise OSError("oversized HTTP CONNECT headers")
        if not response.split(b"\r\n", 1)[0].startswith((b"HTTP/1.0 200", b"HTTP/1.1 200")):
            raise OSError("HTTP CONNECT failed")
        return sock, (time.monotonic() - started) * 1000
    except Exception:
        sock.close()
        raise

def socks5_connect(host, port, target, target_port, timeout):
    preflight()
    sock = socket.create_connection((host, port), timeout)
    started = time.monotonic()
    try:
        sock.sendall(b"\x05\x01\x00")
        if _read(sock, 2) != b"\x05\x00":
            raise OSError("SOCKS5 greeting failed")
        name = target.encode("idna")
        if len(name) > 255 or not 1 <= target_port <= 65535:
            raise ValueError("invalid target")
        sock.sendall(b"\x05\x01\x00\x03" + bytes((len(name),)) + name + target_port.to_bytes(2, "big"))
        reply = _read(sock, 4)
        if reply[0] != 5 or reply[1] != 0:
            raise OSError("SOCKS5 CONNECT failed")
        if reply[3] == 1:
            address_size = 4
        elif reply[3] == 4:
            address_size = 16
        elif reply[3] == 3:
            address_size = _read(sock, 1)[0]
        else:
            raise OSError("invalid SOCKS5 address type")
        _read(sock, address_size + 2)
        return sock, (time.monotonic() - started) * 1000
    except Exception:
        sock.close()
        raise
