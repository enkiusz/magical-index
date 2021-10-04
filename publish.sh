#!/bin/sh

emacs -l publish.el --eval '(org-publish "magical-index")' --eval '(kill-emacs)'
echo 'pop.fsck.pl' > _site/CNAME
if [ "$1" = "--push" ]; then
	(cd _site; git add .; git commit -m 'rebuild'; git push)
fi
