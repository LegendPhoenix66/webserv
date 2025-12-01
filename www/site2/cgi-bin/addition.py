#!/usr/bin/env python3
import os
import sys
import urllib.parse

# Read POST body safely (exact CONTENT_LENGTH bytes)
def read_request_body():
    cl = os.environ.get('CONTENT_LENGTH')
    if not cl:
        return ''
    try:
        n = int(cl)
    except Exception:
        return ''
    if n <= 0:
        return ''
    return sys.stdin.buffer.read(n).decode('utf-8', 'replace')

# Parse parameters from QUERY_STRING (GET) or request body (POST)
def get_params(method, query_string):
    if method == 'GET':
        src = query_string or ''
    else:
        src = read_request_body()
    return urllib.parse.parse_qs(src, keep_blank_values=True)

# Helper to get first value
def first(params, name, default=''):
    v = params.get(name)
    if not v:
        return default
    return v[0]

# Compute result
def compute(a, b, op):
    try:
        # allow integer input but compute in float for division
        if '.' in a or '.' in b:
            x = float(a)
            y = float(b)
        else:
            # try int first for nicer results
            try:
                x = int(a)
                y = int(b)
            except Exception:
                x = float(a)
                y = float(b)
    except Exception:
        raise ValueError('invalid number')

    if op in ('add', '+'):
        return x + y
    if op in ('sub', '-'):
        return x - y
    if op in ('mul', '*', 'x'):
        return x * y
    if op in ('div', '/'):
        if y == 0:
            raise ZeroDivisionError('division by zero')
        return x / y
    if op in ('mod', '%'):
        if y == 0:
            raise ZeroDivisionError('modulo by zero')
        return x % y
    if op in ('pow', '^'):
        return x ** y
    raise ValueError('unsupported operation')

# Build and write CGI response
def write_response(status_code, body_text, content_type='text/plain; charset=utf-8'):
    body_bytes = body_text.encode('utf-8')
    status_line = 'Status: %d %s\r\n' % (status_code, 'OK' if status_code==200 else 'Error')
    headers = (
        status_line +
        'Content-Type: %s\r\n' % content_type +
        'Content-Length: %d\r\n' % len(body_bytes) +
        '\r\n'
    )
    sys.stdout.buffer.write(headers.encode('utf-8') + body_bytes)


def main():
    method = os.environ.get('REQUEST_METHOD', 'GET').upper()
    query = os.environ.get('QUERY_STRING', '')
    params = get_params(method, query)

    x = first(params, 'x', '')
    y = first(params, 'y', '')
    op = first(params, 'op', 'add').lower()

    if x == '' or y == '':
        body = 'error: missing parameter x or y\nusage: x, y, op (add, sub, mul, div, mod, pow)\n'
        write_response(400, body)
        return

    try:
        result = compute(x, y, op)
    except ZeroDivisionError as e:
        write_response(400, 'error: %s\n' % str(e))
        return
    except ValueError as e:
        write_response(400, 'error: %s\n' % str(e))
        return
    except Exception:
        write_response(500, 'error: internal server error\n')
        return

    # Format result: if numeric is integer-like, show as int
    if isinstance(result, float):
        if result.is_integer():
            result_text = str(int(result))
        else:
            result_text = repr(result)
    else:
        result_text = str(result)

    body_lines = []
    body_lines.append('method=' + method)
    body_lines.append('x=' + x)
    body_lines.append('y=' + y)
    body_lines.append('op=' + op)
    body_lines.append('result=' + result_text)
    body_text = '\n'.join(body_lines) + '\n'
    write_response(200, body_text)

if __name__ == '__main__':
    main()
