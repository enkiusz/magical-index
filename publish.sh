#!/bin/sh

emacs --batch -l publish.el --eval '(org-publish "magical-index")'
if [ "$1" = "--push" ]; then
	(cd _site; git add .; git commit -m 'rebuild'; git push)
fi
