#!/usr/bin/env bash

set -euo pipefail

avrdude_bin="${AVRDUDE_BIN:-avrdude}"
programmer="${AVR_PROGRAMMER:-usbasp}"
bitclock="${AVR_BITCLOCK:-10}"

avrdude_args=(
    -c "$programmer"
    -p m644p
    -B "$bitclock"
)

if [[ -n "${AVR_PROGRAMMER_PORT:-}" ]]; then
    avrdude_args+=(-P "$AVR_PROGRAMMER_PORT")
fi

"$avrdude_bin" "${avrdude_args[@]}" \
    -U hfuse:w:0xD9:m \
    -U efuse:w:0xFD:m \
    -U lfuse:w:0xF7:m
