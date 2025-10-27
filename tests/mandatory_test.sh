#!/usr/bin/env bash
# 42 webserv — Mandatory test suite (WSL-friendly)
# Runs a sequence of black-box tests exercising the whole mandatory feature set.
# Requirements: bash, curl, python3, ss (iproute2) or netstat.
# Usage: bash tests/mandatory_test.sh

set -u
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR" || exit 1

PASS=0
FAIL=0
TOTAL=0
TMPDIR="$(mktemp -d -t webserv-tests-XXXXXXXX)" || exit 1
SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill -INT "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
  rm -rf "${TMPDIR}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

say() { printf "%s\n" "$*"; }
ok()  { say "[ OK ] $*"; PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); }
ko()  { say "[FAIL] $*"; FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); }
title(){ printf "\n==== %s ====\n" "$*"; }

have_cmd() { command -v "$1" >/dev/null 2>&1; }

wait_listen() {
  # wait_listen PORT [TIMEOUT_SEC]
  local port="$1"; local timeout="${2:-10}"; local t=0
  while (( t < timeout )); do
    if have_cmd ss; then
      if ss -ltn | awk '{print $4}' | grep -q ":${port}$"; then return 0; fi
    else
      if netstat -an 2>/dev/null | grep -q ":${port} .*LISTEN"; then return 0; fi
    fi
    sleep 0.2; t=$((t+1))
  done
  return 1
}

kill_existing() {
  # Best-effort kill any lingering webserv owned by this user
  if have_cmd pkill; then pkill -INT -x webserv 2>/dev/null || true; fi
}

build() {
  title "Build"
  if make -s; then ok "make completed"; else ko "make failed"; exit 2; fi
}

start_server() {
  # start_server CONFIG [PORT1 [PORT2 ...]]
  local cfg="$1"; shift
  kill_existing
  ./webserv "$cfg" >"${TMPDIR}/server.out" 2>&1 & SERVER_PID=$!
  # Give it a moment to boot
  sleep 0.2
  local p
  for p in "$@"; do
    if ! wait_listen "$p" 50; then
      ko "Server did not start listening on :$p (config=$cfg)"
      say "--- server.out ---"; tail -n +1 "${TMPDIR}/server.out" || true
      return 1
    fi
  done
  ok "Server up (cfg=$cfg, ports=$*)"
}

stop_server() {
  if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill -INT "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
    ok "Server stopped"
  fi
  SERVER_PID=""
}

curl_req() {
  # curl_req METHOD URL [DATA] [EXTRA_CURL_ARGS]
  local method="$1"; local url="$2"; local data="${3:-}"; shift 3 || true
  local hdr="${TMPDIR}/hdr.$$"; local body="${TMPDIR}/body.$$"
  if [[ "$method" == "HEAD" ]]; then
    # Use -I (HEAD) to avoid curl trying to read a body; suppress errors with -s
    curl -s -I -D "$hdr" -o "$body" "$url" "$@"
  elif [[ -n "$data" ]]; then
    curl -s -D "$hdr" -o "$body" -X "$method" "$url" --data-binary "$data" "$@"
  else
    curl -s -D "$hdr" -o "$body" -X "$method" "$url" "$@"
  fi
  local code; code=$(awk 'NR==1{print $2}' "$hdr")
  echo "$code" >"${TMPDIR}/code"; printf "%s\n" "$body" >"${TMPDIR}/last_body_path"
  cat "$hdr" >"${TMPDIR}/last_hdr"
  printf "%s\n" "$code"
}

assert_status() {
  local expected="$1"; local got; got=$(cat "${TMPDIR}/code")
  if [[ "$got" == "$expected" ]]; then ok "status $expected"; else ko "status expected $expected got $got"; fi
}

assert_header_contains() {
  local name="$1"; local needle="$2"
  if grep -i "^$name:" "${TMPDIR}/last_hdr" | grep -q "$needle"; then ok "$name contains '$needle'"; else ko "$name missing '$needle'"; fi
}

assert_body_contains() {
  local needle="$1"; local bodyp; bodyp=$(cat "${TMPDIR}/last_body_path")
  if grep -q "$needle" "$bodyp"; then ok "body contains '$needle'"; else ko "body missing '$needle'"; fi
}

python_send() {
  # python_send HOST PORT RAW_REQUEST_STRING  → prints first status code
  python3 - "$@" <<'PY'
import sys, socket, time
host=sys.argv[1]; port=int(sys.argv[2]); req=sys.argv[3]
s=socket.socket(); s.connect((host,port))
s.sendall(req.encode('ascii', 'ignore'))
s.settimeout(5)
resp=s.recv(1024)
line=resp.split(b"\r\n",1)[0].decode('ascii','ignore')
code=line.split(' ')[1] if ' ' in line else ''
print(code)
PY
}

python_idle_408() {
  # Open TCP then idle past 15s, read response, print code (expect 408)
  python3 - <<'PY'
import socket, time
s=socket.socket(); s.connect(("127.0.0.1",8080))
# do not send headers; just wait
time.sleep(16)
try:
  s.settimeout(2)
  resp=s.recv(1024)
  line=resp.split(b"\r\n",1)[0].decode('ascii','ignore')
  print(line.split(' ')[1])
except Exception:
  print("")
PY
}

# ---------------- Tests ----------------

build

# 1) Static + routing basics
start_server "conf_files/routes_basic.conf" 8080 || exit 1
code=$(curl_req HEAD http://127.0.0.1:8080/)
assert_status 200
assert_header_contains "Content-Type" "text/html"
code=$(curl_req GET http://127.0.0.1:8080/nope)
assert_status 404
curl_req GET http://127.0.0.1:8080/img/icons/insta.png >/dev/null
code=$(cat "${TMPDIR}/code")
assert_status 200
# 405 on POST to default location (GET/HEAD only)
code=$(curl_req POST http://127.0.0.1:8080/ "")
assert_status 405
assert_header_contains "Allow" "GET"
# 414 using raw socket
longpath=$(python3 - <<'PY'
print('/' + 'a'*6000)
PY
)
code=$(python_send 127.0.0.1 8080 "GET ${longpath} HTTP/1.1\r\nHost: x\r\n\r\n")
if [[ "$code" == "414" ]]; then ok "414 long URI"; else ko "expected 414, got $code"; fi
# 408 idle timeout
code=$(python_idle_408)
if [[ "$code" == "408" ]]; then ok "408 timeout"; else ko "expected 408, got $code"; fi
stop_server

# 2) Multi vhost
start_server "conf_files/multi_basic.conf" 8080 || exit 1
code=$(curl_req HEAD http://127.0.0.1:8080/ "" -H 'Host: default.local')
assert_status 200
code=$(curl_req HEAD http://127.0.0.1:8080/ "" -H 'Host: siteb.local')
assert_status 200
stop_server

# 3) Multi ports
start_server "conf_files/multi_ports.conf" 8081 8082 || exit 1
code=$(curl_req HEAD http://127.0.0.1:8081/)
assert_status 200
code=$(curl_req HEAD http://127.0.0.1:8082/)
assert_status 200
stop_server

# 4) Uploads + DELETE + body limits
# Ensure clean uploads dir for predictable result
rm -f www/uploads/hello.txt www/uploads/upload-*.bin 2>/dev/null || true
start_server "conf_files/uploads_basic.conf" 8080 || exit 1
# 411 Length Required (raw POST without CL or chunked)
code=$(python_send 127.0.0.1 8080 $'POST /upload/ HTTP/1.1\r\nHost: x\r\n\r\n')
if [[ "$code" == "411" ]]; then ok "411 length required"; else ko "expected 411, got $code"; fi
# Successful fixed-length upload (201)
code=$(curl_req POST http://127.0.0.1:8080/upload/hello.txt "hello" -H 'Content-Type: text/plain' -H 'Content-Length: 5')
if [[ "$code" == "201" || "$code" == "200" ]]; then ok "POST upload created/overwrote ($code)"; else ko "POST upload expected 201/200, got $code"; fi
# Fetch uploaded file
code=$(curl_req GET http://127.0.0.1:8080/upload/hello.txt)
assert_status 200
assert_body_contains "hello"
# HEAD for uploaded file
code=$(curl_req HEAD http://127.0.0.1:8080/upload/hello.txt)
assert_status 200
# 413 over limit (~2 MiB)
head -c 2000000 /dev/zero >"${TMPDIR}/big.bin"
code=$(curl_req POST http://127.0.0.1:8080/upload/huge.bin @"${TMPDIR}/big.bin" -H 'Content-Type: application/octet-stream' -H 'Content-Length: 2000000')
assert_status 413
# DELETE uploaded file
code=$(curl_req DELETE http://127.0.0.1:8080/upload/hello.txt)
assert_status 204
# Ensure gone
code=$(curl_req GET http://127.0.0.1:8080/upload/hello.txt)
assert_status 404
# Chunked upload (raw)
code=$(python_send 127.0.0.1 8080 $'POST /upload/ HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n6\r\nabcdef\r\n0\r\n\r\n')
if [[ "$code" == "201" || "$code" == "200" ]]; then ok "chunked upload accepted ($code)"; else ko "chunked upload expected 200/201, got $code"; fi
stop_server

# 5) Error page mapping (404 mapped)
start_server "conf_files/error_map.conf" 8080 || exit 1
code=$(curl_req GET http://127.0.0.1:8080/does-not-exist)
assert_status 404
assert_body_contains "404 Not Found"
stop_server

# 6) CGI (Python) + timeout
start_server "conf_files/cgi_python.conf" 8080 || exit 1
code=$(curl_req GET http://127.0.0.1:8080/cgi/)
assert_status 200
assert_body_contains "method= GET"
code=$(curl_req POST http://127.0.0.1:8080/cgi/ "hi" -H 'Content-Type: text/plain' -H 'Content-Length: 2')
assert_status 200
assert_body_contains "body= hi"
stop_server

start_server "conf_files/cgi_timeout.conf" 8080 || exit 1
code=$(curl_req GET http://127.0.0.1:8080/slow/)
assert_status 504
stop_server

# Summary
printf "\n==== SUMMARY ====\nPassed: %d\nFailed: %d\nTotal:  %d\n" "$PASS" "$FAIL" "$TOTAL"
if (( FAIL > 0 )); then exit 1; else exit 0; fi
