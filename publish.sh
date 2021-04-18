#!/bin/sh

emacs --batch -l publish.el --eval '(org-publish "magical-index")'
(cd _site; git add .; git commit -m 'rebuild'; git push)
