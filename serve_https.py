#!/usr/bin/env python3
"""Sirve la carpeta build/ por HTTPS (puerto 8070). Ejecutar desde la raíz del proyecto."""
import functools
import http.server
import ssl

PORT = 8070

handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory="build")
httpd = http.server.ThreadingHTTPServer(("192.168.5.105", PORT), handler)

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.maximum_version = ssl.TLSVersion.TLSv1_2   # compatibilidad con mbedTLS del ESP32
ctx.load_cert_chain("main/certs/server_cert.pem", "server_key.pem")
httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)

print(f"Sirviendo build/ por HTTPS en https://192.168.5.105:{PORT}  (Ctrl+C para salir)")
httpd.serve_forever()
