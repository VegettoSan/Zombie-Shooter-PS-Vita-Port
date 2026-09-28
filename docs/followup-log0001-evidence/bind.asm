  424668:	b5f0      	push	{r4, r5, r6, r7, lr}
  42466a:	af03      	add	r7, sp, #12
  42466c:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  424670:	b0b5      	sub	sp, #212	; 0xd4
  424672:	4604      	mov	r4, r0
  424674:	f8df 03dc 	ldr.w	r0, [pc, #988]	; 424a54
  424678:	469b      	mov	fp, r3
  42467a:	4478      	add	r0, pc
  42467c:	6800      	ldr	r0, [r0, #0]
  42467e:	6800      	ldr	r0, [r0, #0]
  424680:	9034      	str	r0, [sp, #208]	; 0xd0
  424682:	4610      	mov	r0, r2
  424684:	910e      	str	r1, [sp, #56]	; 0x38
  424686:	f807 3cb9 	strb.w	r3, [r7, #-185]
  42468a:	9207      	str	r2, [sp, #28]
  42468c:	f09c e1f0 	blx	8c0a70 // STRING::hash() const
  424690:	6ae5      	ldr	r5, [r4, #44]	; 0x2c
  424692:	4606      	mov	r6, r0
  424694:	f104 0028 	add.w	r0, r4, #40	; 0x28
  424698:	9405      	str	r4, [sp, #20]
  42469a:	9006      	str	r0, [sp, #24]
  42469c:	b375      	cbz	r5, 4246fc
  42469e:	f04f 3055 	mov.w	r0, #1431655765	; 0x55555555
  4246a2:	f04f 3133 	mov.w	r1, #858993459	; 0x33333333
  4246a6:	ea00 0055 	and.w	r0, r0, r5, lsr #1
  4246aa:	1a28      	subs	r0, r5, r0
  4246ac:	ea01 0190 	and.w	r1, r1, r0, lsr #2
