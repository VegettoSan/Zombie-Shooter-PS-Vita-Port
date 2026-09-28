  3f5be4:	b5f0      	push	{r4, r5, r6, r7, lr}
  3f5be6:	af03      	add	r7, sp, #12
  3f5be8:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  3f5bec:	b08d      	sub	sp, #52	; 0x34
  3f5bee:	9100      	str	r1, [sp, #0]
  3f5bf0:	4681      	mov	r9, r0
  3f5bf2:	48e2      	ldr	r0, [pc, #904]	; (3f5f7c
  3f5bf4:	2440      	movs	r4, #64	; 0x40
  3f5bf6:	4478      	add	r0, pc
  3f5bf8:	6800      	ldr	r0, [r0, #0]
  3f5bfa:	6800      	ldr	r0, [r0, #0]
  3f5bfc:	900c      	str	r0, [sp, #48]	; 0x30
  3f5bfe:	2000      	movs	r0, #0
  3f5c00:	900b      	str	r0, [sp, #44]	; 0x2c
  3f5c02:	e9cd 0009 	strd	r0, r0, [sp, #36]	; 0x24
  3f5c06:	4648      	mov	r0, r9
  3f5c08:	f0cc e09a 	blx	8c1d40 // STRING::Length() const
  3f5c0c:	3005      	adds	r0, #5
  3f5c0e:	4284      	cmp	r4, r0
  3f5c10:	d201      	bcs.n	3f5c16
  3f5c12:	3440      	adds	r4, #64	; 0x40
  3f5c14:	e7f7      	b.n	3f5c06
  3f5c16:	4621      	mov	r1, r4
  3f5c18:	9c09      	ldr	r4, [sp, #36]	; 0x24
  3f5c1a:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f5c1c:	9101      	str	r1, [sp, #4]
  3f5c1e:	1b00      	subs	r0, r0, r4
  3f5c20:	4288      	cmp	r0, r1
  3f5c22:	d21a      	bcs.n	3f5c5a
  3f5c24:	f8dd 8004 	ldr.w	r8, [sp, #4]
  3f5c28:	f1b8 3fff 	cmp.w	r8, #4294967295	; 0xffffffff
  3f5c2c:	f340 817b 	ble.w	3f5f26
  3f5c30:	9e0a      	ldr	r6, [sp, #40]	; 0x28
  3f5c32:	4640      	mov	r0, r8
  3f5c34:	f0ca e694 	blx	8c0960 // operator new(unsigned int)
  3f5c38:	1b36      	subs	r6, r6, r4
  3f5c3a:	4621      	mov	r1, r4
  3f5c3c:	4605      	mov	r5, r0
  3f5c3e:	4632      	mov	r2, r6
  3f5c40:	f0c2 e6e0 	blx	8b8a04
  3f5c44:	eb05 0008 	add.w	r0, r5, r8
  3f5c48:	900b      	str	r0, [sp, #44]	; 0x2c
  3f5c4a:	19a8      	adds	r0, r5, r6
  3f5c4c:	2c00      	cmp	r4, #0
  3f5c4e:	e9cd 5009 	strd	r5, r0, [sp, #36]	; 0x24
  3f5c52:	bf1c      	itt	ne
  3f5c54:	4620      	movne	r0, r4
  3f5c56:	f0ca e6cc 	blxne	8c09f0
  3f5c5a:	f0ce e3aa 	blx	8c43b0 // core::crypto::ICryptEngine::instance()
  3f5c5e:	f8dd a028 	ldr.w	sl, [sp, #40]	; 0x28
  3f5c62:	f245 4043 	movw	r0, #21571	; 0x5443
  3f5c66:	f2c4 7041 	movt	r0, #18241	; 0x4741
  3f5c6a:	2400      	movs	r4, #0
  3f5c6c:	9008      	str	r0, [sp, #32]
  3f5c6e:	e008      	b.n	3f5c82
  3f5c70:	a808      	add	r0, sp, #32
  3f5c72:	5d00      	ldrb	r0, [r0, r4]
  3f5c74:	f80a 0b01 	strb.w	r0, [sl], #1
  3f5c78:	3401      	adds	r4, #1
  3f5c7a:	f8cd a028 	str.w	sl, [sp, #40]	; 0x28
  3f5c7e:	2c04      	cmp	r4, #4
  3f5c80:	d035      	beq.n	3f5cee
  3f5c82:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f5c84:	4582      	cmp	sl, r0
  3f5c86:	d3f3      	bcc.n	3f5c70
  3f5c88:	f8dd 8024 	ldr.w	r8, [sp, #36]	; 0x24
  3f5c8c:	ebaa 0b08 	sub.w	fp, sl, r8
  3f5c90:	f10b 0601 	add.w	r6, fp, #1
  3f5c94:	f1b6 3fff 	cmp.w	r6, #4294967295	; 0xffffffff
  3f5c98:	f340 813c 	ble.w	3f5f14
  3f5c9c:	eba0 0008 	sub.w	r0, r0, r8
  3f5ca0:	f06f 4140 	mvn.w	r1, #3221225472	; 0xc0000000
  3f5ca4:	ebb6 0f40 	cmp.w	r6, r0, lsl #1
  3f5ca8:	bf38      	it	cc
  3f5caa:	0046      	lslcc	r6, r0, #1
  3f5cac:	4288      	cmp	r0, r1
  3f5cae:	bf28      	it	cs
  3f5cb0:	f06f 4600 	mvncs.w	r6, #2147483648	; 0x80000000
  3f5cb4:	b126      	cbz	r6, 3f5cc0
  3f5cb6:	4630      	mov	r0, r6
  3f5cb8:	f0ca e652 	blx	8c0960 // operator new(unsigned int)
  3f5cbc:	4605      	mov	r5, r0
  3f5cbe:	e000      	b.n	3f5cc2
  3f5cc0:	2500      	movs	r5, #0
  3f5cc2:	a808      	add	r0, sp, #32
  3f5cc4:	eb05 0a0b 	add.w	sl, r5, fp
  3f5cc8:	4641      	mov	r1, r8
  3f5cca:	465a      	mov	r2, fp
  3f5ccc:	5d00      	ldrb	r0, [r0, r4]
  3f5cce:	f80a 0b01 	strb.w	r0, [sl], #1
  3f5cd2:	4628      	mov	r0, r5
  3f5cd4:	f0c2 e696 	blx	8b8a04
  3f5cd8:	f1b8 0f00 	cmp.w	r8, #0
  3f5cdc:	eb05 0006 	add.w	r0, r5, r6
  3f5ce0:	900b      	str	r0, [sp, #44]	; 0x2c
  3f5ce2:	9509      	str	r5, [sp, #36]	; 0x24
  3f5ce4:	bf1c      	itt	ne
  3f5ce6:	4640      	movne	r0, r8
  3f5ce8:	f0ca e682 	blxne	8c09f0
  3f5cec:	e7c4      	b.n	3f5c78
  3f5cee:	4648      	mov	r0, r9
  3f5cf0:	f0cb e36e 	blx	8c13d0 // STRING::CharPtr() const
  3f5cf4:	f890 a000 	ldrb.w	sl, [r0]
  3f5cf8:	f8dd b028 	ldr.w	fp, [sp, #40]	; 0x28
  3f5cfc:	f1ba 0f00 	cmp.w	sl, #0
  3f5d00:	d03e      	beq.n	3f5d80
  3f5d02:	1c44      	adds	r4, r0, #1
  3f5d04:	e008      	b.n	3f5d18
  3f5d06:	f80b ab01 	strb.w	sl, [fp], #1
  3f5d0a:	f8cd b028 	str.w	fp, [sp, #40]	; 0x28
  3f5d0e:	f814 ab01 	ldrb.w	sl, [r4], #1
  3f5d12:	f1ba 0f00 	cmp.w	sl, #0
  3f5d16:	d033      	beq.n	3f5d80
  3f5d18:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f5d1a:	4583      	cmp	fp, r0
  3f5d1c:	d3f3      	bcc.n	3f5d06
  3f5d1e:	f8dd 8024 	ldr.w	r8, [sp, #36]	; 0x24
  3f5d22:	ebab 0908 	sub.w	r9, fp, r8
  3f5d26:	f109 0601 	add.w	r6, r9, #1
  3f5d2a:	f1b6 3fff 	cmp.w	r6, #4294967295	; 0xffffffff
  3f5d2e:	f340 80f4 	ble.w	3f5f1a
  3f5d32:	eba0 0008 	sub.w	r0, r0, r8
  3f5d36:	f06f 4140 	mvn.w	r1, #3221225472	; 0xc0000000
  3f5d3a:	ebb6 0f40 	cmp.w	r6, r0, lsl #1
  3f5d3e:	bf38      	it	cc
  3f5d40:	0046      	lslcc	r6, r0, #1
  3f5d42:	4288      	cmp	r0, r1
  3f5d44:	bf28      	it	cs
  3f5d46:	f06f 4600 	mvncs.w	r6, #2147483648	; 0x80000000
  3f5d4a:	b126      	cbz	r6, 3f5d56
  3f5d4c:	4630      	mov	r0, r6
  3f5d4e:	f0ca e608 	blx	8c0960 // operator new(unsigned int)
  3f5d52:	4605      	mov	r5, r0
  3f5d54:	e000      	b.n	3f5d58
  3f5d56:	2500      	movs	r5, #0
  3f5d58:	eb05 0b09 	add.w	fp, r5, r9
  3f5d5c:	4628      	mov	r0, r5
  3f5d5e:	4641      	mov	r1, r8
  3f5d60:	464a      	mov	r2, r9
  3f5d62:	f80b ab01 	strb.w	sl, [fp], #1
  3f5d66:	f0c2 e64e 	blx	8b8a04
  3f5d6a:	f1b8 0f00 	cmp.w	r8, #0
  3f5d6e:	eb05 0006 	add.w	r0, r5, r6
  3f5d72:	900b      	str	r0, [sp, #44]	; 0x2c
  3f5d74:	9509      	str	r5, [sp, #36]	; 0x24
  3f5d76:	bf1c      	itt	ne
  3f5d78:	4640      	movne	r0, r8
  3f5d7a:	f0ca e63a 	blxne	8c09f0
  3f5d7e:	e7c4      	b.n	3f5d0a
  3f5d80:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f5d82:	4583      	cmp	fp, r0
  3f5d84:	d204      	bcs.n	3f5d90
  3f5d86:	2000      	movs	r0, #0
  3f5d88:	f80b 0b01 	strb.w	r0, [fp], #1
  3f5d8c:	9e09      	ldr	r6, [sp, #36]	; 0x24
  3f5d8e:	e030      	b.n	3f5df2
  3f5d90:	f8dd 9024 	ldr.w	r9, [sp, #36]	; 0x24
  3f5d94:	ebab 0509 	sub.w	r5, fp, r9
  3f5d98:	1c6c      	adds	r4, r5, #1
  3f5d9a:	f1b4 3fff 	cmp.w	r4, #4294967295	; 0xffffffff
