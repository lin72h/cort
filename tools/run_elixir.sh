#!/bin/sh
set -eu

: "${ERL_CRASH_DUMP:=/dev/null}"
export ERL_CRASH_DUMP

if [ -n "${ELIXIR:-}" ]; then
    exec "$ELIXIR" "$@"
fi

if command -v elixir >/dev/null 2>&1; then
    if elixir --version >/dev/null 2>&1; then
        exec elixir "$@"
    fi
fi

ELIXIR_INSTALLS_ROOT=${ELIXIR_INSTALLS_ROOT:-"$HOME/.elixir-install/installs"}
if [ -d "$ELIXIR_INSTALLS_ROOT/elixir" ] && [ -d "$ELIXIR_INSTALLS_ROOT/otp" ]; then
    for elixir_bin in "$ELIXIR_INSTALLS_ROOT"/elixir/*/bin/elixir; do
        [ -x "$elixir_bin" ] || continue

        elixir_release=$(basename "$(dirname "$(dirname "$elixir_bin")")")
        otp_major=$(printf '%s\n' "$elixir_release" | sed -n 's/.*-otp-\([0-9][0-9]*\)$/\1/p')
        [ -n "$otp_major" ] || continue

        for erl_bin in "$ELIXIR_INSTALLS_ROOT"/otp/"$otp_major"*/bin/erl; do
            [ -x "$erl_bin" ] || continue
            PATH="$(dirname "$erl_bin"):$(dirname "$elixir_bin"):$PATH" \
                exec "$elixir_bin" "$@"
        done
    done
fi

if [ -x /usr/local/lib/elixir/bin/elixir ] && [ -x /usr/local/lib/erlang28/bin/erl ]; then
    PATH="/usr/local/lib/erlang28/bin:/usr/local/lib/elixir/bin:$PATH" \
        exec /usr/local/lib/elixir/bin/elixir "$@"
fi

printf '%s\n' 'unable to start elixir; set ELIXIR or adjust PATH to a matching Erlang/OTP toolchain' >&2
exit 127
