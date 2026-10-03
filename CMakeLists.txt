cmake_minimum_required(VERSION 3.16)

# Versión de la app (se ve en el log y en esp_app_desc_t). Cámbiala en cada build.
set(PROJECT_VER "2.0.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3_ota_demo)
