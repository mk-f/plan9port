#!/bin/sh
set -eux

ACME=${PLAN9}/bin/acme
ACME_AUX=$HOME/.acme

CGO_CFLAGS="-I/usr/local/include/"
CGO_LDFLAGS="-L/usr/local/lib"

MANPATH=$PLAN9/man export MANPATH
PATH=${PLAN9}/bin:$PATH export PATH

pgrep plumber >/dev/null || plumber

if [ "${0##*/}" = "acmebright" ]; then
. "$HOME/src/acme-scripts/solarized-bright.theme"
fi

if [ "${0##*/}" = "acmedark" ]; then
. "$HOME/src/acme-scripts/solarized-dark.theme"
fi

#	-f /mnt/font/GoMono/10a/font

if [ "${0##*/}" = "acmedark" ] || [ "${0##*/}" = "acmebright" ]; then
	TEMPLATE=$(printf '%s' \
		"textback:$TXTBG,"\
		"texthigh:$TXTHLBG,"\
		"textbord:$TXTBORD,"\
		"texttext:$TXTFG,"\
		"texthtext:$TXTHLFG,"\
		"tagback:$TAGBG,"\
		"taghigh:$TAGHLBG,"\
		"tagbord:$TAGBORD,"\
		"tagtext:$TAGFG,"\
		"taghtext:$TAGHLFG,"\
		"butmod:$BUTMOD,"\
		"butcol:$BUTCOL,"\
		"but2col:$B2HL,"\
		"but3col:$B3HL")

#	SHELL=${PLAN9}/bin/rc $ACME -c 1 -a -M \
	# rc starts as loginshell, reading $home/lib/profile
	# when argument 0 (its name) starts with '-'. -rc is a
	# link to rc
	SHELL=${PLAN9}/bin/rc $ACME -c 2 -a \
			-f ${PLAN9}/font/fixed/unicode.9x15.font \
			-t "$TEMPLATE" \
			"$@" $ACME_AUX/guide
else
#	SHELL=${PLAN9}/bin/rc $ACME -c 1 -a -M \
	SHELL=${PLAN9}/bin/rc $ACME -c 1 -a \
		-f ${PLAN9}/font/fixed/unicode.9x15.font \
		"$@" $ACME_AUX/guide
fi
