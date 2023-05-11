#!/bin/sh
#
# Convert an scdoc file to GitHub markdown, with some custom tweaks to work
# around pandoc's poor formatting.
#
# Usage:
# ./scd2gfm < file.scd > file.md

set -eu

html=$(scd2html -f)
no_sections=$(echo "$html" | sed -e "s|</\?section.*||" -e "s/[^;]<a.*//")
italics=$(echo "$no_sections" | sed "s|<u>\([^<]*\)</u>|<i>\1</i>|g")
no_header=$(echo "$italics" | awk '
/<header>/ {
	header = 1
}

/<\/header>/ {
	header = 0
}

!/header/ {
	if (header == 0) {
		print
	}
}
')

combine_blockquotes=$(echo "$no_header" | awk '
/\/blockquote/ {
	if (endblock > 0) {
		print buffer
	}
	endblock = NR
	buffer = $0
}

/<blockquote/ {
	if (endblock == NR - 2) {
		print "<br/> <br/>"
		gsub("<blockquote>", "")
		buffer = ""
	} else {
		print buffer
		print
		buffer = ""
	}
}

!/blockquote/ {
	if (endblock == NR - 2) {
		print buffer
		buffer = ""
	}
	print
}
')

echo "$combine_blockquotes" | pandoc --from html --to gfm | sed "s/\s\+$//"
