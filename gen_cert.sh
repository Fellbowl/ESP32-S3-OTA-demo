#!/bin/bash
# Genera el certificado autofirmado del servidor OTA.
#  - main/certs/server_cert.pem -> se EMBEBE en el firmware (certificate pinning)
#  - server_key.pem             -> clave privada, SOLO para el servidor (no se sube a git)
# Ejecutar UNA vez. Si regeneras el cert, hay que recompilar y reflashear por USB.
set -e
cd "$(dirname "$0")"
mkdir -p main/certs
openssl req -x509 -newkey rsa:2048 -sha256 -days 825 -nodes \
  -keyout server_key.pem -out main/certs/server_cert.pem \
  -subj "/CN=esp32-ota-demo"
echo "OK: main/certs/server_cert.pem y server_key.pem generados"
