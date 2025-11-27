#!/usr/bin/env python3
import os, sys
body = sys.stdin.read()
print("Status: 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n", end="")
print("method=", os.environ.get("REQUEST_METHOD"))
print("query=", os.environ.get("QUERY_STRING"))
print("len=", os.environ.get("CONTENT_LENGTH"))
print("body=", body)
