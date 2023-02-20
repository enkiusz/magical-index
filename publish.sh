#!/bin/sh

if [ "$1" = "--push" ]; then
	# First pull from origin not to cause conflicts
	(cd _site; git pull)
fi

emacs -l publish.el --eval '(org-publish "magical-index")' --eval '(kill-emacs)'
echo 'pop.fsck.pl' > _site/CNAME
if [ "$1" = "--push" ]; then
	(cd _site; git add .; git commit -m 'rebuild'; git push)
fi
