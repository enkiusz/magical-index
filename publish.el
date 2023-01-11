(require 'ox)
  ( org-babel-do-load-languages 'org-babel-load-languages '((gnuplot . t)) )
  ( org-babel-do-load-languages 'org-babel-load-languages '((python . t)) )
  ( org-babel-do-load-languages 'org-babel-load-languages '((shell . t)) )

(setf user-full-name "Maciej Grela")
(setf user-mail-address "enki@fsck.pl")

(setq org-publish-project-alist
      '(
        ("magical-index" :components ("magical-index-pages" "magical-index-static" "magical-index-archive"))
        ("magical-index-pages"
         :base-directory "."
         :base-extension "org"
         :exclude "_site/"
         :publishing-directory "_site/"
         :recursive t
         :publishing-function org-html-publish-to-html
         :auto-sitemap t
         :sitemap-title "A Certain Magical Index"
         :with-author t
         :with-date nil
         :with-creator nil
         :language "en"
         :html-metadata-timestamp-format "%Y-%m-%d %H:%M"
         )

        ("magical-index-static"
         :base-directory "."
         :base-extension "css\\|js\\|svg\\|pdf\\|bin\\|rpm\\|deb\\|png\\|webp\\|jpg\\|jpeg\\|gif\\|apk\\|tar.xz\\|tar.bz2\\|tar.gz\\|zip\\|txt\\|py\\|sh\\|config\\|kicad.*\\|logicdata\\|ino\\|csv\\|pcapng\\|sal"
         :exclude "_site/"
         :publishing-directory "_site/"
         :recursive t
         :with-author t
         :with-date nil
         :with-creator nil
         :publishing-function org-publish-attachment
         )

        ("magical-index-archive"
         :base-directory "."
         :base-extension 'any
         :exclude "_site/"
         :publishing-directory "_site/projects/archive"
         :recursive t
         :with-author t
         :with-date nil
         :with-creator nil
         :language "en"
         :publishing-function org-publish-attachment
         )
	)
)
