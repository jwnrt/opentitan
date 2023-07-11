#!/usr/bin/env bash

# Dependencies:
#
# * formatjson5
# * ripgrep
# * sed

# Collect Hjson files.
files=$(fd -e hjson)

# Various counters for inspection.
valid_json5=0
invalid_json5=0
corrections=0

for file in $files; do
  printf "${file} "

  # formatjson5 only gives one error at a time, so keep working a file until it's done.
  while true; do
    # Run formatjson5 over the file, capturing stderr.
    stderr="$(formatjson5 "$file" 2>&1 1>/dev/null)"
  
    # If the formatter succeeded, we've got no issues. Take a tally.
    if [ "$?" -eq 0 ]; then
      ((valid_json5++))
      printf ' [OK]'
      break
    fi
  
    # Check for a missing comma error
    line_no="$(rg -o '(\d+):\d+:' -r '$1' <<< "$stderr")"
  
    # Check for missing comma errors:
    if rg -q '(Properties|Array items) must be separated by a comma' <<< "$stderr"; then
      # Can't deal with 'key: http://' yet - skip 'em.
      if rg -q 'http[s]:' <<< "$stderr" || rg -q '\\$' <<< "$stderr"; then
        break
      fi

      # Walk backwards over empty lines and comments...
      while
        ((line_no--))
        line="$(sed -n "${line_no}p;" $file)"
        rg -q '^\s*$' <<< "$line" || rg -q '^\s*/[/*].*$' <<< "$line" || rg -q '^\s*[*].*$' <<< "$line"
      do true; done
  
      # Add a comma to the end of this line
      sed -E -i "$line_no"'s/^(([^/"]|"[^"]*")*)(\s*\/\/.*)?$/\1,\3/' $file

      # Progress bar :)
      ((corrections++))
      printf ","
    elif rg -q 'Unexpected token' <<< "$stderr"; then
      if rg -q '#' <<< "$stderr"; then
        sed -i "$line_no"'s/#/\/\//' $file

        # Progress bar :)
        ((corrections++))
        printf "#"
      elif rg -q "'''" <<< "$stderr"; then
        echo "multiline string"
        break
      elif rg -q '\[' <<< "$stderr"; then
        echo "$stderr" >&2
        break
      else
        unquoted="$(tail -n +2 <<< "$stderr" | head -n 1 | rg -o '^\s*([^:]+:\s*)?(.*)$' -r '$2')"
        # Get the char that the unquoted string starts at
        char_no="$(rg -o '\d+:(\d+):' -r '$1' <<< "$stderr")"
        len="$(wc -c <<< "$unquoted")"
        ((char_no--))
        ((len--))
        sed -E -i "${line_no}s/^(.{${char_no}})(.{${len}})/"'\1"\2"/' "$file"

        # Progress bar :)
        ((corrections++))
        printf '"'
      fi
    else
      # Might not be valid JSON5, but at least it's not a comma error.
      printf ' [SKIP]'
      echo "${stderr}"
      ((invalid_json5++))
      break
    fi
  done

  printf "\n"
done

echo "${valid_json5} files are valid JSON5"
echo "${invalid_json5} files are invalid JSON5"

echo "Made ${corrections} corrections"
