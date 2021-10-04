#!/bin/sh

emacs -l publish.el --eval '(org-publish "magical-index")' --eval '(kill-emacs)'
if [ "$1" = "--push" ]; then
	(cd _site; git add .; git commit -m 'rebuild'; git push)
fi
