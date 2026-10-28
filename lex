#!/bin/sh
set -eu

if [ "$#" -eq 0 ] || [ "${1:-}" = '--help' ] || [ "${1:-}" = '-h' ]; then
    printf 'Usage: ./lex NAME [analyzer options]\n'
    printf 'Examples: ./lex sample | ./lex average | ./lex braces --braces\n'
    printf 'Use a name from examples, or a file path. The .clx extension is optional.\n'
    exit 0
fi

project_dir=$(CDPATH= cd -P "$(dirname "$0")" && pwd)
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) analyzer="$project_dir/bin/clarifix.exe" ;;
    *) analyzer="$project_dir/bin/clarifix" ;;
esac
if [ ! -x "$analyzer" ]; then
    printf 'Build the analyzer first with ./build.\n' >&2
    exit 2
fi

source_name=$1
shift
case "${source_name##*/}" in
    *.*) ;;
    *) source_name="$source_name.clx" ;;
esac
source_path=
for candidate in "$source_name" "$project_dir/$source_name" "$project_dir/examples/$source_name"; do
    if [ -f "$candidate" ]; then
        source_path=$candidate
        break
    fi
done
if [ -z "$source_path" ]; then
    printf "Cannot find '%s'. Save it in examples or provide its path.\n" "$source_name" >&2
    exit 2
fi

exec "$analyzer" "$@" "$source_path"
