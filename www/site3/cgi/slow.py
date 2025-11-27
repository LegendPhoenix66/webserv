#!/usr/bin/env python3
import time, sys
# Sleep long enough to exceed the server's CGI timeout (~5s)
time.sleep(10)
# If it wasn't killed by timeout, output a valid CGI response
sys.stdout.write("Status: 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nslow done")
