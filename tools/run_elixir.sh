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

if [ -x /usr/local/lib/elixir/bin/elixir ] && [ -x /usr/local/lib/erlang28/bin/erl ]; then
    PATH="/usr/local/lib/erlang28/bin:/usr/local/lib/elixir/bin:$PATH" \
        exec /usr/local/lib/elixir/bin/elixir "$@"
fi

printf '%s\n' 'unable to start elixir; set ELIXIR or adjust PATH to a matching Erlang/OTP toolchain' >&2
exit 127
