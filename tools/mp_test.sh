#!/bin/sh
# Two instances (host + client) in one private network namespace; output in work/mp_test/{host,client}
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; O="$ROOT/work/mp_test"; mkdir -p "$O/host" "$O/client"
cp "$ROOT/work/shots/mp_host.txt" "$O/host/s.txt"; cp "$ROOT/work/shots/mp_client.txt" "$O/client/s.txt"
unshare -rn sh -c '
ip link set lo up; ip addr add 192.168.1.125/24 dev lo; ip addr add 192.168.1.126/24 dev lo
run() { RT_IPX_PEERS=192.168.1.125,192.168.1.126 RT_HOST_IP=$1 RT_CALLS_FILE="$3/calls.txt" RT_RESOLUTION=800x600 RT_NO_GRAB=1 RT_NO_VIDEO=1 RT_SCRIPT="$3/s.txt" RT_DUMP_DIR="$3" RT_DEBUG=1 RT_DEBUG_FILE="$3/debug.log" timeout -s KILL 150 "'$ROOT'/build_trace/aztec_recomp" "'$ROOT'/game/Aztec.exe" >"$3/out.txt" 2>&1; }
run 192.168.1.125 x "'$O'/host" &
sleep 1
run 192.168.1.126 x "'$O'/client" &
wait'
