
; gEDA - GNU Electronic Design Automation
; parse-gerber.scm 
; Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
;
; $Id$
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA



; These are data that is the result of the parsing

; *aperture-description* is a list with descriptions of all 
; apertures used.  Each element of the list is a list with:
; (<aperture no> <aperture type> <apterture size(s)>)
; <aperture type> is one of circle, rectangle, oval or polygon.
; <apterture size(s)> is one value if circle and two if rectangle.
(define *aperture-description* '())

; *netlist* is the actual netlist. Each element of the list consist of
; (<X> <Y> (<aperture no> . <exposure type>))
; The elements in the list is 'backwards', the first element in the
; list is the last in the file. This is simply fixed by a
; (reverse *netlist*)
(define *netlist* '())

; Image parameters
(define *image* 
  (list (cons 'unit 'inches)
	(cons 'polarity 'positive)
	(cons 'omit-leading-zeros #t)
	(cons 'coordinates 'absolute)
	(cons 'x-integer 2)
	(cons 'x-decimal 3)
	(cons 'y-integer 2)
	(cons 'y-decimal 3)))

(define *current-x* #f)
(define *current-y* #f)

(define (set-aperture-size aperture size)
  (if (number? size)
      (cons size (cdr aperture))
      (display "Wrong aperture number in set-aperture-size!\n")))

(define (set-aperture-state aperture state)
  (if (or (eq? state 'exposure-off)
	  (eq? state 'exposure-on)
	  (eq? state 'exposure-flash))
      (cons (car aperture) state)
      (display "Wrong state in set-aperture-state!\n")))

(define *current-aperture* (cons #f #f))

; Main function. Call this one.
(define (parse-gerber port)
  (let ((latest (read-char port)))
    (if (not (eof-object? latest))
	(case latest
	  ((#\G)
	   (parse-G-code port)
	   (parse-gerber port))
	  ((#\D)
	   (set! *current-aperture* (parse-D-code port *current-aperture*))
	   (parse-gerber port))
	  ((#\M)
	   (parse-M-code port)
	   (parse-gerber port))
	  ((#\X)
	   (set! *current-x* (list->string (parse-pos-code port)))
	   (parse-gerber port))
	  ((#\Y)
	   (set! *current-y* (list->string (parse-pos-code port)))
	   (parse-gerber port))
	  ((#\%)
	   (parse-274X-code port)
	   (parse-gerber port))
	  ((#\*)
	   (if (and *current-x*  ; If all these set
		    *current-y* 
		    (car *current-aperture*)
		    (cdr *current-aperture*))
	       (begin 
;		 (display *current-x*) 
;		 (display " : ")
;		 (display *current-y*) 
;		 (display " A:")
;		 (display *current-aperture*)
;		 (newline)
		 (set! *netlist* 
		       (cons (list *current-x* *current-y* *current-aperture*)
			     *netlist*))
		 (parse-gerber port))))
	  (else  ; Eat dead meat
;Whitespaces keeps us away from using this
;	   (display "Strange code : ") 
;	   (display latest)
;	   (newline)
	   (parse-gerber port))))))


(define (parse-G-code port)
  (let* ((first (read-char port))
	 (second (read-char port))
	 (code (string first second)))
    (cond ((string=? code "00") ; Move
	   ); Not implemented
	  ((string=? code "01") ; Linear Interpolation (1X scale)
	   ); Not implemented
	  ((string=? code "02") ; Clockwise Linear Interpolation
	   ); Not implemented
	  ((string=? code "03") ; Counter Clockwise Linear Interpolation
	   ); Not implemented
	  ((string=? code "04") ; Ignore Data Block
	   (eat-til-eob port)
	   (parse-gerber port))
	  ((string=? code "10") ; Linear Interpolation (10X scale)
	   ); Not implemented
	  ((string=? code "11") ; Linear Interpolation (0.1X scale)
	   ); Not implemented
	  ((string=? code "12") ; Linear Interpolation (0.01X scale)
	   ); Not implemented
	  ((string=? code "36") ; Turn on Polygon Area Fill
	   ); Not implemented
	  ((string=? code "37") ; Turn off Polygon Area Fill
	   ); Not implemented
	  ((string=? code "54") ; Tool prepare
	   (if (char=? (read-char port) #\D)
	       (set! *current-aperture* (parse-D-code port *current-aperture*)))
	   (eat-til-eob port))
	  ((string=? code "70") ; Specify inches
	   ); Not implemented
	  ((string=? code "71") ; Specify millimeters
	   ); Not implemented
	  ((string=? code "74") ; Disable 360 circular interpolation
	   ); Not implemented
	  ((string=? code "75") ; Enable 360 circular interpolation
	   ); Not implemented
	  ((string=? code "90") ; Specify absolut format
	   ); Not implemented
	  ((string=? code "91") ; Specify incremental format
	   ); Not implemented
	  (else
	   (display "Strange G-code : ")
	   (display code)
	   (newline)))))


(define (parse-M-code port)
  (let* ((first (read-char port))
	 (second (read-char port))
	 (code (string first second)))
    (cond ((string=? code "00") ; Program Stop
	   ); The file ends anyhow
	  ((string=? code "01") ; Optional Stop
	   ); The file ends anyhow
	  ((string=? code "02") ; End Of Program
	   ); The file ends anyhow
	  (else
	   (display "Strange M code")
	   (newline)))))


(define (parse-D-code port current-aperture)
  (let ((aperture (list->string (parse-pos-code port))))
    (cond ((string=? aperture "1") ; Exposure on
	   (set-aperture-state current-aperture 'exposure-on))
	  ((string=? aperture "2") ; Exposure off
	   (set-aperture-state aperture 'exposure-off))
	  ((string=? aperture "3") ; Flash aperture
	   (set-aperture-state current-aperture 'exposure-flash))
	  ((string=? aperture "01") ; Exposure on
	   (set-aperture-state current-aperture 'exposure-on))
 	  ((string=? aperture "02") ; Exposure off
	   (set-aperture-state current-aperture 'exposure-off))
	  ((string=? aperture "03") ; Flash aperture
	   (set-aperture-state current-aperture 'exposure-flash))
	  (else ; Select an aperture defined by an AD parameter
	   (set-aperture-size current-aperture (string->number aperture))))))


(define (parse-274X-code port)
  (let* ((first (read-char port))
	 (second (read-char port))
	 (code (string first second)))
          ; Directive parameters
    (cond ((string=? code "AS") ; Axis Select
	   (display "AS")
	   (eat-til-eop port))
	  ((string=? code "FS") ; Format Statement
	   (if (char=? (read-char port) #\L) ; or T
	       (assoc-set! *image* 'omit-leading-zeros #t)
	       (assoc-set! *image* 'omit-leading-zeros #f))
	   (if (char=? (read-char port) #\A) ; or I
	       (assoc-set! *image* 'coordinates 'absolut)
	       (assoc-set! *image* 'coordinates 'incremental))
	   (read-char port) ; eat X
	   (assoc-set! *image* 'x-int (char->integer(read-char port)))
	   (assoc-set! *image* 'x-dec (char->integer(read-char port)))
	   (read-char port) ; eat Y
	   (assoc-set! *image* 'y-int (char->integer(read-char port)))
	   (assoc-set! *image* 'y-dec (char->integer(read-char port)))
	   (eat-til-eop port))
	  ((string=? code "MI") ; Mirror Image
	   (display "MI")
	   (eat-til-eop port))
	  ((string=? code "MO") ; Mode of units
	   (let ((unit (string (read-char port) (read-char port))))
	     (cond ((string=? unit "IN")
		    (assoc-set! *image* 'unit 'inch))
		   ((string=? unit "MM")
		    (assoc-set! *image* 'unit 'mm))))
	   (eat-til-eop port))
	  ((string=? code "OF") ; Offset
	   (display "OF")
	   (eat-til-eop port))
	  ((string=? code "SF") ; Scale Factor
	   (display "SF")
	   (eat-til-eop port))
          ; Image parameters
	  ((string=? code "IJ") ; Image Justify
	   (eat-til-eop port))
	  ((string=? code "IN") ; Image Name
	   (eat-til-eop port))
	  ((string=? code "IO") ; Image Offset
	   (eat-til-eop port))
	  ((string=? code "IP") ; Image Polarity
	   (let ((unit (string (read-char port) (read-char port) (read-char port))))
	     (cond ((string=? unit "POS")
		    (assoc-set! *image* 'polarity 'positive))
		   ((string=? unit "NEG")
		    (assoc-set! *image* 'polarity 'negative))))
	   (eat-til-eop port))
	  ((string=? code "IR") ; Image Rotation
	   (eat-til-eop port))
	  ((string=? code "PF") ; Plotter Film
	   (eat-til-eop port))
          ; Aperture Parameters
	  ((string=? code "AD") ; Aperture Description
	   (read-char port) ; Read the D. Should check that it really is a D.
	   (set! *aperture-description* 
		 (cons (cons (car (parse-D-code port '(#f .#f)))
			     (parse-aperture-definition port))
		       *aperture-description*))
	   (eat-til-eop port))
	  ((string=? code "AM") ; Aperture Macro
	   (eat-til-eop port))
	  (else
	   (eat-til-eop port)))))


(define (parse-aperture-definition port)
  (let ((aperture-type (read-char port))
	(read-comma (read-char port)))
     (case aperture-type
       ((#\C) ; Circle
	(cons 'circle (parse-modifier port)))
       ((#\R) ; Rectangle or Square
	(cons 'rectangle (parse-modifier port)))
       ((#\O) ; Oval
	(cons 'oval (parse-modifier port)))
       ((#\P) ; Polygon
	(cons 'polygon (parse-modifier port)))
       (else
	#f))))

     
(define (parse-modifier port)
  (let ((dimension (list->string (parse-pos-code port)))
	(next (peek-char port)))
    (if (char=? next #\X)
	(begin
	  (read-char port)
	  (cons dimension (parse-modifier port)))
	(cons dimension '()))))

	
(define (eat-til-eop port)
  (let ((latest (read-char port)))
      (if (not (char=? latest #\%))
	  (eat-til-eop port))))


(define (parse-pos-code port) ; Returns a list with all characters in position
  (let ((sneak (peek-char port)))
    (if (or (and (char>=? sneak #\0)
		 (char<=? sneak #\9))
	    (char=? sneak #\.))
	(cons (read-char port) (parse-pos-code port))
	'())))


(define (eat-til-eob port)
  (if (not (char=? #\* (read-char port)))
      (eat-til-eob port)))
