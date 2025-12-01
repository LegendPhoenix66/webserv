#!/usr/bin/env python3
import os, sys

def main():
    # Read only the number of bytes indicated by CONTENT_LENGTH to avoid blocking
    try:
        content_length = int(os.environ.get("CONTENT_LENGTH") or "0")
    except ValueError:
        content_length = 0

    body_bytes = b""
    if content_length > 0:
        # Read exactly content_length bytes from the raw buffer (bytes)
        body_bytes = sys.stdin.buffer.read(content_length)

    # Decode for display; use replacement for undecodable bytes
    body_text_from_body = body_bytes.decode('utf-8', 'replace')

    body_lines = []
    body_lines.append("method=" + (os.environ.get("REQUEST_METHOD") or ""))
    body_lines.append("query=" + (os.environ.get("QUERY_STRING") or ""))
    body_lines.append("len=" + str(content_length))
    body_lines.append("body=" + body_text_from_body)
    body_text = "\n".join(body_lines) + "\n"

    # Encode to bytes so Content-Length is the byte-length (UTF-8 safe)
    body_bytes_out = body_text.encode('utf-8')
    headers = "Status: 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n\r\n" % len(body_bytes_out)

    # Write headers+body as bytes to avoid any encoding confusion
    sys.stdout.buffer.write(headers.encode('utf-8') + body_bytes_out)


if __name__ == '__main__':
    main()
