;;; Help --- Show help on guild commands

;;;; 	Copyright (C) 2009, 2010, 2011 Free Software Foundation, Inc.
;;;; 
;;;; This library is free software; you can redistribute it and/or
;;;; modify it under the terms of the GNU Lesser General Public
;;;; License as published by the Free Software Foundation; either
;;;; version 3 of the License, or (at your option) any later version.
;;;; 
;;;; This library is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;;; Lesser General Public License for more details.
;;;; 
;;;; You should have received a copy of the GNU Lesser General Public
;;;; License along with this library; if not, write to the Free
;;;; Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
;;;; Boston, MA 02110-1301 USA

;;; Commentary:

;; Usage: help
;;
;; Show help for Guild scripts.

;;; Code:

(define-module (scripts help)
  #:use-module (ice-9 format)
  #:use-module (ice-9 documentation)
  #:use-module ((srfi srfi-1) #:select (fold append-map)))

(define %summary "Show a brief help message.")


(define (directory-files dir)
  (if (and (file-exists? dir) (file-is-directory? dir))
      (let ((dir-stream (opendir dir)))
        (let loop ((new (readdir dir-stream))
                   (acc '()))
          (if (eof-object? new)
              (begin
                (closedir dir-stream)
                acc)
              (loop (readdir dir-stream)
                    (if (or (string=? "."  new)             ; ignore
                            (string=? ".." new))            ; ignore
                        acc
                        (cons new acc))))))
      '()))

(define (strip-extensions path)
  (or-map (lambda (ext)
            (and
             (string-suffix? ext path)
             ;; We really can't be adding e.g. ChangeLog-2008 to the set
             ;; of runnable scripts, just because "" is a valid
             ;; extension, by default.  So hack around that here.
             (not (string-null? ext))
             (substring path 0
                        (- (string-length path) (string-length ext)))))
          (append %load-compiled-extensions %load-extensions)))

(define (unique l)
  (cond ((null? l) l)
        ((null? (cdr l)) l)
        ((equal? (car l) (cadr l)) (unique (cdr l)))
        (else (cons (car l) (unique (cdr l))))))

(define (find-submodules head)
  (let ((shead (map symbol->string head)))
    (unique
     (sort
      (append-map (lambda (path)
                    (fold (lambda (x rest)
                            (let ((stripped (strip-extensions x)))
                              (if stripped (cons stripped rest) rest)))
                          '()
                          (directory-files
                           (fold (lambda (x y) (in-vicinity y x)) path shead))))
                  %load-path)
      string<?))))

(define (list-commands all?)
  (display "\
Usage: guild COMMAND [ARGS]

  guild runs command-line scripts provided by GNU Guile and related
  programs.  See \"Using Guile Tools\" in the Guile manual, for more
  information.

Commands:
")

  (for-each
   (lambda (name)
     (let* ((modname `(scripts ,(string->symbol name)))
            (mod (resolve-module modname #:ensure #f))
            (summary (and mod (and=> (module-variable mod '%summary)
                                     variable-ref))))
       (if (and mod
                (or all?
                    (let ((v (module-variable mod '%include-in-guild-list)))
                      (if v (variable-ref v) #t))))
           (if summary
               (format #t "  ~A ~23t~a\n" name summary)
               (format #t "  ~A\n" name)))))
   (find-submodules '(scripts)))
  (display "
For help on a specific command, try \"guild help COMMAND\".
"))

(define (module-commentary mod)
  (file-commentary
   (%search-load-path (module-filename mod))))

(define (main . args)
  (cond
   ((null? args)
    (list-commands #f))
   ((or (equal? args '("--all")) (equal? args '("-a")))
    (list-commands #t))
   ((not (string-prefix? "-" (car args)))
    ;; help for particular command
    (let* ((name (car args))
           (mod (resolve-module `(scripts ,(string->symbol name))
                                #:ensure #f)))
      (if mod
          (let ((commentary (module-commentary mod)))
            (if commentary
                (display commentary)
                (format #t "No documentation found for command \"~a\".\n"
                        name)))
          (begin
            (format #t "No command named \"~a\".\n" name)
            (exit 1)))))
   (else
    (display "Usage: guild help
       guild help --all
       guild help COMMAND

Show a help on guild commands.  With --all, show arcane incantations as
well.  With COMMAND, show more detailed help for a particular command.
")
    (exit 1))))
