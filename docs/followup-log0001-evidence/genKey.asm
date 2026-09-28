  3f5ab0:	b5f0      	push	{r4, r5, r6, r7, lr}
  3f5ab2:	af03      	add	r7, sp, #12
  3f5ab4:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  3f5ab8:	b087      	sub	sp, #28
  3f5aba:	9001      	str	r0, [sp, #4]
  3f5abc:	4614      	mov	r4, r2
  3f5abe:	4847      	ldr	r0, [pc, #284]	; (3f5bdc
  3f5ac0:	4478      	add	r0, pc
  3f5ac2:	6800      	ldr	r0, [r0, #0]
  3f5ac4:	6800      	ldr	r0, [r0, #0]
  3f5ac6:	9006      	str	r0, [sp, #24]
  3f5ac8:	2000      	movs	r0, #0
  3f5aca:	9005      	str	r0, [sp, #20]
  3f5acc:	e9cd 0003 	strd	r0, r0, [sp, #12]
  3f5ad0:	4610      	mov	r0, r2
  3f5ad2:	f0cb e2de 	blx	8c1090 // STRING::isEmpty() const
  3f5ad6:	b340      	cbz	r0, 3f5b2a
  3f5ad8:	a903      	add	r1, sp, #12
  3f5ada:	2000      	movs	r0, #0
  3f5adc:	f000 fcc6 	bl	3f646c
  3f5ae0:	a903      	add	r1, sp, #12
  3f5ae2:	2001      	movs	r0, #1
  3f5ae4:	f000 fcc2 	bl	3f646c
  3f5ae8:	a903      	add	r1, sp, #12
  3f5aea:	2002      	movs	r0, #2
  3f5aec:	f000 fcbe 	bl	3f646c
  3f5af0:	a903      	add	r1, sp, #12
  3f5af2:	2003      	movs	r0, #3
  3f5af4:	f000 fcba 	bl	3f646c
  3f5af8:	e9dd 1003 	ldrd	r1, r0, [sp, #12]
  3f5afc:	1a42      	subs	r2, r0, r1
  3f5afe:	9801      	ldr	r0, [sp, #4]
  3f5b00:	f0cb e766 	blx	8c19d0 // core::crypto::sha256(unsigned char const*, unsigned int)
  3f5b04:	9803      	ldr	r0, [sp, #12]
  3f5b06:	2800      	cmp	r0, #0
  3f5b08:	bf1c      	itt	ne
  3f5b0a:	9004      	strne	r0, [sp, #16]
  3f5b0c:	f0ca e770 	blxne	8c09f0
  3f5b10:	9806      	ldr	r0, [sp, #24]
  3f5b12:	4933      	ldr	r1, [pc, #204]	; (3f5be0
  3f5b14:	4479      	add	r1, pc
  3f5b16:	6809      	ldr	r1, [r1, #0]
  3f5b18:	6809      	ldr	r1, [r1, #0]
  3f5b1a:	4281      	cmp	r1, r0
  3f5b1c:	bf02      	ittt	eq
  3f5b1e:	b007      	addeq	sp, #28
  3f5b20:	e8bd 0f00 	ldmiaeq.w	sp!, {r8, r9, sl, fp}
  3f5b24:	bdf0      	popeq	{r4, r5, r6, r7, pc}
  3f5b26:	f0ca e754 	blx	8c09d0 // __stack_chk_fail
  3f5b2a:	2500      	movs	r5, #0
  3f5b2c:	9402      	str	r4, [sp, #8]
  3f5b2e:	e007      	b.n	3f5b40
  3f5b30:	f809 5b01 	strb.w	r5, [r9], #1
  3f5b34:	4655      	mov	r5, sl
  3f5b36:	9c02      	ldr	r4, [sp, #8]
  3f5b38:	f8cd 9010 	str.w	r9, [sp, #16]
  3f5b3c:	f10a 0501 	add.w	r5, sl, #1
  3f5b40:	4620      	mov	r0, r4
  3f5b42:	f0cc e0fe 	blx	8c1d40 // STRING::Length() const
  3f5b46:	4285      	cmp	r5, r0
  3f5b48:	d8c6      	bhi.n	3f5ad8
  3f5b4a:	4620      	mov	r0, r4
  3f5b4c:	4629      	mov	r1, r5
  3f5b4e:	46aa      	mov	sl, r5
  3f5b50:	f0ce e12e 	blx	8c3db0 // STRING::operator[](unsigned int) const
  3f5b54:	4605      	mov	r5, r0
  3f5b56:	e9dd 9004 	ldrd	r9, r0, [sp, #16]
  3f5b5a:	4581      	cmp	r9, r0
  3f5b5c:	d3e8      	bcc.n	3f5b30
  3f5b5e:	9c03      	ldr	r4, [sp, #12]
  3f5b60:	eba9 0b04 	sub.w	fp, r9, r4
  3f5b64:	f10b 0601 	add.w	r6, fp, #1
  3f5b68:	f1b6 3fff 	cmp.w	r6, #4294967295	; 0xffffffff
  3f5b6c:	dd26      	ble.n	3f5bbc
  3f5b6e:	1b00      	subs	r0, r0, r4
  3f5b70:	f06f 4140 	mvn.w	r1, #3221225472	; 0xc0000000
  3f5b74:	ebb6 0f40 	cmp.w	r6, r0, lsl #1
  3f5b78:	bf38      	it	cc
  3f5b7a:	0046      	lslcc	r6, r0, #1
  3f5b7c:	4288      	cmp	r0, r1
  3f5b7e:	bf28      	it	cs
  3f5b80:	f06f 4600 	mvncs.w	r6, #2147483648	; 0x80000000
  3f5b84:	b126      	cbz	r6, 3f5b90
  3f5b86:	4630      	mov	r0, r6
  3f5b88:	f0ca e6ea 	blx	8c0960 // operator new(unsigned int)
  3f5b8c:	4680      	mov	r8, r0
  3f5b8e:	e001      	b.n	3f5b94
  3f5b90:	f04f 0800 	mov.w	r8, #0
  3f5b94:	eb08 090b 	add.w	r9, r8, fp
  3f5b98:	4640      	mov	r0, r8
  3f5b9a:	4621      	mov	r1, r4
  3f5b9c:	465a      	mov	r2, fp
  3f5b9e:	f809 5b01 	strb.w	r5, [r9], #1
  3f5ba2:	f0c2 e730 	blx	8b8a04
  3f5ba6:	2c00      	cmp	r4, #0
  3f5ba8:	eb08 0006 	add.w	r0, r8, r6
  3f5bac:	9005      	str	r0, [sp, #20]
  3f5bae:	f8cd 800c 	str.w	r8, [sp, #12]
  3f5bb2:	bf1c      	itt	ne
  3f5bb4:	4620      	movne	r0, r4
  3f5bb6:	f0ca e71c 	blxne	8c09f0
  3f5bba:	e7bb      	b.n	3f5b34
  3f5bbc:	a803      	add	r0, sp, #12
  3f5bbe:	f7fc fb7d 	bl	3f22bc
  3f5bc2:	e002      	b.n	3f5bca
  3f5bc4:	e001      	b.n	3f5bca
  3f5bc6:	e000      	b.n	3f5bca
  3f5bc8:	e7ff      	b.n	3f5bca
  3f5bca:	9803      	ldr	r0, [sp, #12]
  3f5bcc:	2800      	cmp	r0, #0
  3f5bce:	bf1c      	itt	ne
  3f5bd0:	9004      	strne	r0, [sp, #16]
  3f5bd2:	f0ca e70e 	blxne	8c09f0
  3f5bd6:	f0ca e704 	blx	8c09e0 // __cxa_end_cleanup
  3f5bdc:	9d58      	ldr	r5, [sp, #352]	; 0x160
  3f5bde:	0054      	lsls	r4, r2, #1
  3f5be0:	9d04      	ldr	r5, [sp, #16]
  3f5be2:	0054      	lsls	r4, r2, #1
