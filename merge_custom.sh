#!/bin/sh
set -e

# merge selected remote-branches into a new local branch.
#
# do this in two steps to avoid getting asked for credentials
# multiple itmes

usage()
{
	printf "usage: %s <branch-to-create> [-f]\n" "${0##*/}"
	printf "\t-f deletes all branches and syncs before merging again\n"
	exit 1
}

[ $# -lt 1 ] && usage
new_branch="${1}"
force="${2}"

git checkout master

# remote branches to merge into $local
set -- 	acme/argv-theme \
	acme/keyboard_ctrl \
	acme/soft-tabs \
	9term/darkmode

remotes="$*"

if [ -n "$force" ]; then
	git fetch
	for remote in $remotes; do
		git branch -D "${remote}" || true
	done

	git branch -D "${new_branch}" || true
fi
	

for remote in $remotes; do
	git branch --track "${remote}" "origin/${remote}" || true
done

git checkout -b "${new_branch}"

for remote in $remotes; do
	git merge --no-edit "${remote}"
done
