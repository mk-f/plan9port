#!/bin/sh
set -e

# merge selected remote-branches into a new local branch.
#
# do this in two steps to avoid getting asked for credentials
# multiple itmes

usage()
{
	printf "usage: %s <branch-to-create>\n" "${0##*/}"
	exit 1
}

[ $# -ne 1 ] && usage
new_branch="${1}"

# remote branches to merge into $local
set -- 	acme/dark-theme \
	acme/keyboard_ctrl \
	acme/soft-tabs \
	acme/spaces_tabs_via_9p \
	9term/win 

remotes="$*"

for remote in $remotes; do
	git branch --track "${remote}" "origin/${remote}"
done

git checkout -b "${new_branch}"

for remote in $remotes; do
	git merge --no-edit "${remote}"
done
