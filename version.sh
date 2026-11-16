#!/bin/bash
tmp=$(mktemp)
jq '.version = "'$1'"' ./library.json > "$tmp"
mv "$tmp" ./library.json
sed -i "s/^version=.*/version=$1/" library.properties
