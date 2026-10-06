; ui_blit.asm - inner loop of the Focus glyph drawing (ui_font.c ui_draw_glyph), in assembly:
; KhiCAS runs from flash, where every instruction fetch is slow, and the compiled C loop takes
; about 4 times as many instructions per pixel.
;
; void ui_blit2(unsigned char * dst, const unsigned char * bits, unsigned char mi,
;               unsigned char w, unsigned char rows, const unsigned char * ramp);
;
; Draws rows x w pixels of a 2-bit glyph (4 pixels per byte, high bits first, rows back to back,
; the first pixel at pixel mi of *bits) at dst (screen rows of 320 bytes). Level 0 is
; transparent; levels 1-3 are drawn as ramp[1..3] over ramp[0]. Where the screen already holds
; a shade of the same ramp, the darker one is kept (overlapping glyph boxes). w, rows >= 1.
;
; Registers: HL screen, DE bits, C remaining bits of the current byte (current pixel on top),
; B pixels left in the row, IXL pixels left in the byte, IXH level of the current pixel (64,
; 128 or 192). IY: frame; after setup the argument slots hold
;   (iy+9) r0, (iy+10) r1, (iy+11) r2, (iy+12) r3, (iy+13) scratch, (iy+15) w, (iy+18) rows
;   left, (iy+21) row stride (320 - w).

	assume	adl=1

	section	.text

	public	_ui_blit2

_ui_blit2:
	push	ix
	ld	iy, 0
	add	iy, sp			; (iy+6) dst (iy+9) bits (iy+12) mi (iy+15) w (iy+18) rows (iy+21) ramp
	ld	de, (iy + 9)		; DE = bits
	ld	a, (iy + 12)
	ld	ixl, a			; mi, for now
	ld	hl, (iy + 21)		; the ramp, copied into the frame
	ld	a, (hl)
	ld	(iy + 9), a
	inc	hl
	ld	a, (hl)
	ld	(iy + 10), a
	inc	hl
	ld	a, (hl)
	ld	(iy + 11), a
	inc	hl
	ld	a, (hl)
	ld	(iy + 12), a
	ld	hl, 320			; row stride
	ld	bc, 0
	ld	c, (iy + 15)
	or	a, a
	sbc	hl, bc
	ld	(iy + 21), hl
	ld	hl, (iy + 6)		; HL = dst
	ld	a, (de)			; the first byte, its first mi pixels skipped
	inc	de
	ld	c, a
	ld	a, ixl
	or	a, a
	jr	z, .aligned
	ld	b, a
.skipmi:
	sla	c
	sla	c
	djnz	.skipmi
.aligned:
	ld	a, 4
	sub	a, ixl
	ld	ixl, a			; pixels left in this byte
	ld	b, (iy + 15)		; pixels left in the row

.pix:
	ld	a, ixl
	or	a, a
	jr	nz, .bits
	ld	a, (de)			; next byte
	inc	de
	ld	c, a
	ld	ixl, 4
.bits:
	ld	a, c
	or	a, a
	jr	z, .skip		; the rest of this byte is transparent
	and	a, 192
	jr	z, .next		; this pixel is transparent
	ld	ixh, a
	ld	a, (hl)
	cp	a, (iy + 9)
	jr	nz, .overlap		; not the background: keep the darker shade
.write:
	ld	a, ixh
	cp	a, 192
	jr	z, .w3
	cp	a, 128
	ld	a, (iy + 11)
	jr	z, .w
	ld	a, (iy + 10)
	jr	.w
.w3:
	ld	a, (iy + 12)
.w:
	ld	(hl), a
.next:
	inc	hl
	sla	c
	sla	c
	dec	ixl
	djnz	.pix

.rowend:
	push	de
	ld	de, (iy + 21)
	add	hl, de
	pop	de
	ld	b, (iy + 15)
	dec	(iy + 18)
	jr	nz, .pix
	pop	ix
	ret

.skip:					; skip n = min(pixels left in the byte, in the row)
	ld	a, ixl
	cp	a, b
	jr	c, .skipn
	ld	a, b
.skipn:
	ld	(iy + 13), a
	push	de
	ld	de, 0
	ld	e, a
	add	hl, de
	pop	de
	ld	a, ixl
	sub	a, (iy + 13)
	ld	ixl, a
	ld	a, b
	sub	a, (iy + 13)
	ld	b, a
	jr	nz, .pix
	jr	.rowend

.overlap:				; A = the screen pixel
	cp	a, (iy + 12)
	jr	z, .next		; full ink already
	cp	a, (iy + 11)
	jr	nz, .ov1
	ld	a, ixh			; over level 2: only level 3 is darker
	cp	a, 192
	jr	nz, .next
	jr	.write
.ov1:
	cp	a, (iy + 10)
	jr	nz, .write		; not a shade of this ramp: drawn over
	ld	a, ixh			; over level 1: levels 2 and 3 are darker
	cp	a, 64
	jr	z, .next
	jr	.write
