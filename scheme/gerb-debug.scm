; gEDA - GNU Electronic Design Automation
; gerb-ps.scm 
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


(define *last-aperture-type* '())
(define *last-x* '())
(define *last-y* '())

(define (net:get-start net)
  (list-ref net 0))
(define (net:get-stop net)
  (list-ref net 1))
(define (net:get-aperture net)
  (list-ref net 2))
(define (net:get-interpolation net)
  (list-ref net 3))
(define (net:get-cirseg net)
  (list-ref net 4))

(define (aperture:get-number aperture)
  (list-ref aperture 0))
(define (aperture:get-type aperture)
  (list-ref aperture 1))
(define (aperture:get-sizes aperture)
  (list-tail aperture 2))


      
(define (main netlist aperture info format filename)
  (display "APERTURE:\n")
  (display aperture)
  (newline)
  (display "INFO:\n")
  (display info)
  (newline)
  (display "FORMAT:\n")
  (display format)
  (newline)

  (display "NETLIST:\n")
  (for-each (lambda (element)
	      (display element)
	      (newline))
	    (reverse netlist)))
