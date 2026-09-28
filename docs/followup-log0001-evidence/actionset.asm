  41faa4:	b5f0      	push	{r4, r5, r6, r7, lr}
  41faa6:	af03      	add	r7, sp, #12
  41faa8:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  41faac:	b085      	sub	sp, #20
  41faae:	4681      	mov	r9, r0
  41fab0:	4844      	ldr	r0, [pc, #272]	; (41fbc4
  41fab2:	4690      	mov	r8, r2
  41fab4:	4478      	add	r0, pc
  41fab6:	6800      	ldr	r0, [r0, #0]
  41fab8:	6800      	ldr	r0, [r0, #0]
  41faba:	9004      	str	r0, [sp, #16]
  41fabc:	f8d9 5018 	ldr.w	r5, [r9, #24]
  41fac0:	9101      	str	r1, [sp, #4]
  41fac2:	2d00      	cmp	r5, #0
  41fac4:	d051      	beq.n	41fb6a
  41fac6:	f04f 3055 	mov.w	r0, #1431655765	; 0x55555555
  41faca:	460e      	mov	r6, r1
  41facc:	ea00 0055 	and.w	r0, r0, r5, lsr #1
  41fad0:	f04f 3133 	mov.w	r1, #858993459	; 0x33333333
  41fad4:	1a28      	subs	r0, r5, r0
  41fad6:	ea01 0190 	and.w	r1, r1, r0, lsr #2
  41fada:	f020 30cc 	bic.w	r0, r0, #3435973836	; 0xcccccccc
  41fade:	4408      	add	r0, r1
  41fae0:	f04f 3101 	mov.w	r1, #16843009	; 0x1010101
  41fae4:	eb00 1010 	add.w	r0, r0, r0, lsr #4
  41fae8:	f020 30f0 	bic.w	r0, r0, #4042322160	; 0xf0f0f0f0
  41faec:	4348      	muls	r0, r1
  41faee:	ea4f 6b10 	mov.w	fp, r0, lsr #24
  41faf2:	f1bb 0f01 	cmp.w	fp, #1
  41faf6:	d803      	bhi.n	41fb00
  41faf8:	1e68      	subs	r0, r5, #1
  41fafa:	ea00 0a06 	and.w	sl, r0, r6
  41fafe:	e007      	b.n	41fb10
  41fb00:	42b5      	cmp	r5, r6
  41fb02:	46b2      	mov	sl, r6
  41fb04:	d804      	bhi.n	41fb10
  41fb06:	4630      	mov	r0, r6
  41fb08:	4629      	mov	r1, r5
  41fb0a:	f098 e78e 	blx	8b8a28
  41fb0e:	468a      	mov	sl, r1
  41fb10:	f8d9 0014 	ldr.w	r0, [r9, #20]
  41fb14:	f850 002a 	ldr.w	r0, [r0, sl, lsl #2]
  41fb18:	b338      	cbz	r0, 41fb6a
  41fb1a:	6804      	ldr	r4, [r0, #0]
  41fb1c:	b32c      	cbz	r4, 41fb6a
  41fb1e:	f8cd 8000 	str.w	r8, [sp]
  41fb22:	f1a5 0801 	sub.w	r8, r5, #1
  41fb26:	e004      	b.n	41fb32
  41fb28:	68a0      	ldr	r0, [r4, #8]
  41fb2a:	42b0      	cmp	r0, r6
  41fb2c:	d013      	beq.n	41fb56
  41fb2e:	6824      	ldr	r4, [r4, #0]
  41fb30:	b184      	cbz	r4, 41fb54
  41fb32:	6860      	ldr	r0, [r4, #4]
  41fb34:	42b0      	cmp	r0, r6
  41fb36:	d0f7      	beq.n	41fb28
  41fb38:	f1bb 0f01 	cmp.w	fp, #1
  41fb3c:	d802      	bhi.n	41fb44
  41fb3e:	ea00 0008 	and.w	r0, r0, r8
  41fb42:	e005      	b.n	41fb50
  41fb44:	42a8      	cmp	r0, r5
  41fb46:	d303      	bcc.n	41fb50
  41fb48:	4629      	mov	r1, r5
  41fb4a:	f098 e76e 	blx	8b8a28
  41fb4e:	4608      	mov	r0, r1
  41fb50:	4550      	cmp	r0, sl
  41fb52:	d0ec      	beq.n	41fb2e
  41fb54:	2400      	movs	r4, #0
  41fb56:	f8dd 8000 	ldr.w	r8, [sp]
  41fb5a:	f1b8 0f00 	cmp.w	r8, #0
  41fb5e:	d108      	bne.n	41fb72
  41fb60:	a901      	add	r1, sp, #4
  41fb62:	4648      	mov	r0, r9
  41fb64:	f0a9 e37c 	blx	8c9260 // unsigned int std::__ndk1::__hash_table<input::ActionStates::Type, std::__ndk1::hash<input::ActionStates::Type>, std::__ndk1::equal_to<input::ActionStates::Type>, std::__ndk1::allocator<input::ActionStates::Type> >::__erase_unique<input::ActionStates::Type>(input::ActionStates::Type const&)
  41fb68:	e00e      	b.n	41fb88
  41fb6a:	2400      	movs	r4, #0
  41fb6c:	f1b8 0f00 	cmp.w	r8, #0
  41fb70:	d0f6      	beq.n	41fb60
  41fb72:	2c00      	cmp	r4, #0
  41fb74:	bf1c      	itt	ne
  41fb76:	7b20      	ldrbne	r0, [r4, #12]
  41fb78:	2800      	cmpne	r0, #0
  41fb7a:	d10a      	bne.n	41fb92
  41fb7c:	aa01      	add	r2, sp, #4
  41fb7e:	a802      	add	r0, sp, #8
  41fb80:	4649      	mov	r1, r9
  41fb82:	4613      	mov	r3, r2
  41fb84:	f0a3 e12c 	blx	8c2de0 // std::__ndk1::pair<std::__ndk1::__hash_iterator<std::__ndk1::__hash_node<input::ActionStates::Type, void*>*>, bool> std::__ndk1::__hash_table<input::ActionStates::Type, std::__ndk1::hash<input::ActionStates::Type>, std::__ndk1::equal_to<input::ActionStates::Type>, std::__ndk1::allocator<input::ActionStates::Type> >::__emplace_unique_key_args<input::ActionStates::Type, input::ActionStates::Type const&>(input::ActionStates::Type const&, input::ActionStates::Type const&)
  41fb88:	2c00      	cmp	r4, #0
  41fb8a:	bf1c      	itt	ne
  41fb8c:	7b20      	ldrbne	r0, [r4, #12]
  41fb8e:	4540      	cmpne	r0, r8
  41fb90:	d10c      	bne.n	41fbac
  41fb92:	9804      	ldr	r0, [sp, #16]
  41fb94:	490c      	ldr	r1, [pc, #48]	; (41fbc8
  41fb96:	4479      	add	r1, pc
  41fb98:	6809      	ldr	r1, [r1, #0]
  41fb9a:	6809      	ldr	r1, [r1, #0]
  41fb9c:	4281      	cmp	r1, r0
  41fb9e:	bf02      	ittt	eq
  41fba0:	b005      	addeq	sp, #20
  41fba2:	e8bd 0f00 	ldmiaeq.w	sp!, {r8, r9, sl, fp}
  41fba6:	bdf0      	popeq	{r4, r5, r6, r7, pc}
  41fba8:	f0a0 e712 	blx	8c09d0 // __stack_chk_fail
  41fbac:	f884 800c 	strb.w	r8, [r4, #12]
  41fbb0:	f1b8 0f00 	cmp.w	r8, #0
  41fbb4:	bf1e      	ittt	ne
  41fbb6:	f899 003c 	ldrbne.w	r0, [r9, #60]	; 0x3c
  41fbba:	3001      	addne	r0, #1
  41fbbc:	f889 003c 	strbne.w	r0, [r9, #60]	; 0x3c
  41fbc0:	e7e7      	b.n	41fb92
  41fbc4:	fd64 0051 	stc2l	0, cr0, [r4, #-324]!	; 0xfffffebc
  41fbc8:	fc82 0051 	stc2	0, cr0, [r2], {81}	; 0x51
