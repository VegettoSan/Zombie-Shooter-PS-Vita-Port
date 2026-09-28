  3e2be8:	b5f0      	push	{r4, r5, r6, r7, lr}
  3e2bea:	af03      	add	r7, sp, #12
  3e2bec:	f84d 8d04 	str.w	r8, [sp, #-4]!
  3e2bf0:	4604      	mov	r4, r0
  3e2bf2:	2000      	movs	r0, #0
  3e2bf4:	2900      	cmp	r1, #0
  3e2bf6:	e9c4 0000 	strd	r0, r0, [r4]
  3e2bfa:	60a0      	str	r0, [r4, #8]
  3e2bfc:	bf1c      	itt	ne
  3e2bfe:	4690      	movne	r8, r2
  3e2c00:	2a00      	cmpne	r2, #0
  3e2c02:	d012      	beq.n	3e2c2a
  3e2c04:	2020      	movs	r0, #32
  3e2c06:	460e      	mov	r6, r1
  3e2c08:	f0dd e6aa 	blx	8c0960 // operator new(unsigned int)
  3e2c0c:	efc0 0050 	vmov.i32	q8, #0	; 0x00000000
  3e2c10:	4605      	mov	r5, r0
  3e2c12:	f940 0a0d 	vst1.8	{d16-d17}, [r0]!
  3e2c16:	f940 0a0d 	vst1.8	{d16-d17}, [r0]!
  3e2c1a:	e9c4 5000 	strd	r5, r0, [r4]
  3e2c1e:	60a0      	str	r0, [r4, #8]
  3e2c20:	4630      	mov	r0, r6
  3e2c22:	4641      	mov	r1, r8
  3e2c24:	462a      	mov	r2, r5
  3e2c26:	f0e1 e41c 	blx	8c4460 // SHA256
  3e2c2a:	f85d 8b04 	ldr.w	r8, [sp], #4
  3e2c2e:	bdf0      	pop	{r4, r5, r6, r7, pc}
  3e2c30:	4628      	mov	r0, r5
  3e2c32:	6065      	str	r5, [r4, #4]
  3e2c34:	f0dd e6dc 	blx	8c09f0 // operator delete(void*)
  3e2c38:	f0dd e6d2 	blx	8c09e0 // __cxa_end_cleanup
  3e2c3c:	b5f0      	push	{r4, r5, r6, r7, lr}
  3e2c3e:	af03      	add	r7, sp, #12
  3e2c40:	f84d 8d04 	str.w	r8, [sp, #-4]!
  3e2c44:	2900      	cmp	r1, #0
  3e2c46:	4604      	mov	r4, r0
  3e2c48:	bf1c      	itt	ne
  3e2c4a:	4690      	movne	r8, r2
  3e2c4c:	2a00      	cmpne	r2, #0
  3e2c4e:	d106      	bne.n	3e2c5e
  3e2c50:	2000      	movs	r0, #0
  3e2c52:	e9c4 0000 	strd	r0, r0, [r4]
  3e2c56:	60a0      	str	r0, [r4, #8]
  3e2c58:	f85d 8b04 	ldr.w	r8, [sp], #4
  3e2c5c:	bdf0      	pop	{r4, r5, r6, r7, pc}
  3e2c5e:	2014      	movs	r0, #20
  3e2c60:	460e      	mov	r6, r1
  3e2c62:	f0dd e67e 	blx	8c0960 // operator new(unsigned int)
  3e2c66:	4605      	mov	r5, r0
  3e2c68:	3014      	adds	r0, #20
  3e2c6a:	efc0 0050 	vmov.i32	q8, #0	; 0x00000000
  3e2c6e:	60a0      	str	r0, [r4, #8]
  3e2c70:	e9c4 5000 	strd	r5, r0, [r4]
  3e2c74:	4628      	mov	r0, r5
  3e2c76:	2100      	movs	r1, #0
  3e2c78:	f940 0a0d 	vst1.8	{d16-d17}, [r0]!
  3e2c7c:	6001      	str	r1, [r0, #0]
  3e2c7e:	4630      	mov	r0, r6
  3e2c80:	4641      	mov	r1, r8
  3e2c82:	462a      	mov	r2, r5
  3e2c84:	f0e1 e3e4 	blx	8c4450 // SHA1
  3e2c88:	f85d 8b04 	ldr.w	r8, [sp], #4
  3e2c8c:	bdf0      	pop	{r4, r5, r6, r7, pc}
  3e2c8e:	4628      	mov	r0, r5
  3e2c90:	6065      	str	r5, [r4, #4]
  3e2c92:	f0dd e6ae 	blx	8c09f0 // operator delete(void*)
  3e2c96:	f0dd e6a4 	blx	8c09e0 // __cxa_end_cleanup
  3e2c9a:	b5f0      	push	{r4, r5, r6, r7, lr}
  3e2c9c:	af03      	add	r7, sp, #12
  3e2c9e:	f84d 8d04 	str.w	r8, [sp, #-4]!
  3e2ca2:	2900      	cmp	r1, #0
  3e2ca4:	4604      	mov	r4, r0
  3e2ca6:	bf1c      	itt	ne
  3e2ca8:	4690      	movne	r8, r2
  3e2caa:	2a00      	cmpne	r2, #0
  3e2cac:	d106      	bne.n	3e2cbc
  3e2cae:	2000      	movs	r0, #0
  3e2cb0:	e9c4 0000 	strd	r0, r0, [r4]
  3e2cb4:	60a0      	str	r0, [r4, #8]
  3e2cb6:	f85d 8b04 	ldr.w	r8, [sp], #4
  3e2cba:	bdf0      	pop	{r4, r5, r6, r7, pc}
  3e2cbc:	2020      	movs	r0, #32
  3e2cbe:	460e      	mov	r6, r1
  3e2cc0:	f0dd e64e 	blx	8c0960 // operator new(unsigned int)
  3e2cc4:	efc0 0050 	vmov.i32	q8, #0	; 0x00000000
  3e2cc8:	4605      	mov	r5, r0
  3e2cca:	f940 0a0d 	vst1.8	{d16-d17}, [r0]!
  3e2cce:	f940 0a0d 	vst1.8	{d16-d17}, [r0]!
  3e2cd2:	e9c4 5000 	strd	r5, r0, [r4]
  3e2cd6:	60a0      	str	r0, [r4, #8]
  3e2cd8:	4630      	mov	r0, r6
  3e2cda:	4641      	mov	r1, r8
  3e2cdc:	462a      	mov	r2, r5
  3e2cde:	f0e1 e3c0 	blx	8c4460 // SHA256
  3e2ce2:	f85d 8b04 	ldr.w	r8, [sp], #4
  3e2ce6:	bdf0      	pop	{r4, r5, r6, r7, pc}
  3e2ce8:	4628      	mov	r0, r5
  3e2cea:	6065      	str	r5, [r4, #4]
  3e2cec:	f0dd e680 	blx	8c09f0 // operator delete(void*)
  3e2cf0:	f0dd e676 	blx	8c09e0 // __cxa_end_cleanup
  3e2cf4:	4770      	bx	lr
  3e2cf6:	defe      	udf	#254	; 0xfe
  3e2cf8:	4770      	bx	lr
  3e2cfa:	4770      	bx	lr
  3e2cfc:	2000      	movs	r0, #0
  3e2cfe:	4770      	bx	lr
  3e2d00:	2100      	movs	r1, #0
  3e2d02:	e9c0 1100 	strd	r1, r1, [r0]
  3e2d06:	6081      	str	r1, [r0, #8]
  3e2d08:	4770      	bx	lr
  3e2d0a:	2100      	movs	r1, #0
  3e2d0c:	e9c0 0000 	strd	r0, r0, [r0]
  3e2d10:	6081      	str	r1, [r0, #8]
  3e2d12:	4770      	bx	lr
  3e2d14:	4802      	ldr	r0, [pc, #8]	; (3e2d20
  3e2d16:	4478      	add	r0, pc
  3e2d18:	6800      	ldr	r0, [r0, #0]
  3e2d1a:	6800      	ldr	r0, [r0, #0]
  3e2d1c:	4770      	bx	lr
  3e2d20:	cbb6      	ldmia	r3!, {r1, r2, r4, r5, r7}
  3e2d22:	0055      	lsls	r5, r2, #1
  3e2d24:	f100 0108 	add.w	r1, r0, #8
  3e2d28:	2201      	movs	r2, #1
  3e2d2a:	efc0 0050 	vmov.i32	q8, #0	; 0x00000000
