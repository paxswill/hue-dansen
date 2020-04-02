#!/usr/bin/env bash

set -euo pipefail
# debug mode:
#set -x

OPENSSL="${OPENSSL_PREFIX:=/usr}/bin/openssl"

PSK="$(hexdump -n 16 -e '4/4 "%08X"' /dev/random)"

function start_server() {
	coproc OPENSSL_SERVER (${OPENSSL} s_server \
		-dtls \
		-accept 2100 \
		-nocert \
		-psk ${PSK}
	)
}

function stop_server() {
	kill ${OPENSSL_SERVER_PID}
}

function read_server_output() {
	local server_output
	local output=()
	#local OLDIFS="${IFS}"
	#IFS=""
	while read -t 0.1 -u "${OPENSSL_SERVER[0]}" server_output; do
		output+=("$(printf "%s" "$server_output")")
	done
	for ((i=0; i < ${#output[@]}; i+=1)); do
		echo "${output[$i]}"
	done
	#printf "%s" "${output[@]}"
	#IFS="${OLDIFS}"
}
