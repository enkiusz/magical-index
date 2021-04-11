#!/bin/sh

emacs --batch -l publish.el --eval '(org-publish "magical-index")'
