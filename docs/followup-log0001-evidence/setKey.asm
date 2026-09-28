  424520:	b5f0      	push	{r4, r5, r6, r7, lr}
  424522:	af03      	add	r7, sp, #12
  424524:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  424528:	b087      	sub	sp, #28
  42452a:	4b4c      	ldr	r3, [pc, #304]	; (42465c
  42452c:	29fe      	cmp	r1, #254	; 0xfe
  42452e:	447b      	add	r3, pc
  424530:	681b      	ldr	r3, [r3, #0]
  424532:	681b      	ldr	r3, [r3, #0]
  424534:	9306      	str	r3, [sp, #24]
  424536:	d85f      	bhi.n	4245f8
  424538:	f807 1c2e 	strb.w	r1, [r7, #-46]
  42453c:	b342      	cbz	r2, 424590
  42453e:	2a01      	cmp	r2, #1
  424540:	d15a      	bne.n	4245f8
  424542:	f1a1 0220 	sub.w	r2, r1, #32
  424546:	6844      	ldr	r4, [r0, #4]
  424548:	2a60      	cmp	r2, #96	; 0x60
  42454a:	460a      	mov	r2, r1
  42454c:	bf28      	it	cs
  42454e:	2200      	movcs	r2, #0
  424550:	f880 2088 	strb.w	r2, [r0, #136]	; 0x88
  424554:	b3bc      	cbz	r4, 4245c6
  424556:	f04f 3255 	mov.w	r2, #1431655765	; 0x55555555
  42455a:	f04f 3333 	mov.w	r3, #858993459	; 0x33333333
  42455e:	ea02 0254 	and.w	r2, r2, r4, lsr #1
  424562:	1aa2      	subs	r2, r4, r2
  424564:	ea03 0392 	and.w	r3, r3, r2, lsr #2
  424568:	f022 32cc 	bic.w	r2, r2, #3435973836	; 0xcccccccc
  42456c:	441a      	add	r2, r3
  42456e:	f04f 3301 	mov.w	r3, #16843009	; 0x1010101
  424572:	eb02 1212 	add.w	r2, r2, r2, lsr #4
  424576:	f022 32f0 	bic.w	r2, r2, #4042322160	; 0xf0f0f0f0
  42457a:	435a      	muls	r2, r3
  42457c:	ea4f 6812 	mov.w	r8, r2, lsr #24
  424580:	f1b8 0f01 	cmp.w	r8, #1
  424584:	d809      	bhi.n	42459a
  424586:	f104 02ff 	add.w	r2, r4, #255	; 0xff
  42458a:	ea02 0b01 	and.w	fp, r2, r1
  42458e:	e012      	b.n	4245b6
  424590:	f1a7 012e 	sub.w	r1, r7, #46	; 0x2e
  424594:	f0a4 e744 	blx	8c9420 // unsigned int std::__ndk1::__hash_table<std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::__unordered_map_hasher<unsigned char, std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::hash<unsigned char>, std::__ndk1::equal_to<unsigned char>, true>, std::__ndk1::__unordered_map_equal<unsigned char, std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::equal_to<unsigned char>, std::__ndk1::hash<unsigned char>, true>, std::__ndk1::allocator<std::__ndk1::__hash_value_type<unsigned char, unsigned short> > >::__erase_unique<unsigned char>(unsigned char const&)
  424598:	e02e      	b.n	4245f8
  42459a:	428c      	cmp	r4, r1
  42459c:	468b      	mov	fp, r1
  42459e:	d80a      	bhi.n	4245b6
  4245a0:	b2ca      	uxtb	r2, r1
  4245a2:	b2e3      	uxtb	r3, r4
  4245a4:	4605      	mov	r5, r0
  4245a6:	460e      	mov	r6, r1
  4245a8:	4610      	mov	r0, r2
  4245aa:	4619      	mov	r1, r3
  4245ac:	f094 e23c 	blx	8b8a28
  4245b0:	468b      	mov	fp, r1
  4245b2:	4628      	mov	r0, r5
  4245b4:	4631      	mov	r1, r6
  4245b6:	6802      	ldr	r2, [r0, #0]
  4245b8:	f852 202b 	ldr.w	r2, [r2, fp, lsl #2]
  4245bc:	2a00      	cmp	r2, #0
  4245be:	bf1c      	itt	ne
  4245c0:	6816      	ldrne	r6, [r2, #0]
  4245c2:	2e00      	cmpne	r6, #0
  4245c4:	d125      	bne.n	424612
  4245c6:	4926      	ldr	r1, [pc, #152]	; (424660
  4245c8:	f1a7 062d 	sub.w	r6, r7, #45	; 0x2d
  4245cc:	f1a7 022e 	sub.w	r2, r7, #46	; 0x2e
  4245d0:	4604      	mov	r4, r0
  4245d2:	4479      	add	r1, pc
  4245d4:	9203      	str	r2, [sp, #12]
  4245d6:	680b      	ldr	r3, [r1, #0]
  4245d8:	a903      	add	r1, sp, #12
  4245da:	e9cd 1600 	strd	r1, r6, [sp]
  4245de:	a904      	add	r1, sp, #16
  4245e0:	4608      	mov	r0, r1
  4245e2:	4621      	mov	r1, r4
  4245e4:	f0a4 e724 	blx	8c9430 // std::__ndk1::pair<std::__ndk1::__hash_iterator<std::__ndk1::__hash_node<std::__ndk1::__hash_value_type<unsigned char, unsigned short>, void*>*>, bool> std::__ndk1::__hash_table<std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::__unordered_map_hasher<unsigned char, std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::hash<unsigned char>, std::__ndk1::equal_to<unsigned char>, true>, std::__ndk1::__unordered_map_equal<unsigned char, std::__ndk1::__hash_value_type<unsigned char, unsigned short>, std::__ndk1::equal_to<unsigned char>, std::__ndk1::hash<unsigned char>, true>, std::__ndk1::allocator<std::__ndk1::__hash_value_type<unsigned char, unsigned short> > >::__emplace_unique_key_args<unsigned char, std::__ndk1::piecewise_construct_t const&, std::__ndk1::tuple<unsigned char const&>, std::__ndk1::tuple<> >(unsigned char const&, std::__ndk1::piecewise_construct_t const&, std::__ndk1::tuple<unsigned char const&>&&, std::__ndk1::tuple<>&&)
  4245e8:	9804      	ldr	r0, [sp, #16]
  4245ea:	2201      	movs	r2, #1
  4245ec:	f894 105d 	ldrb.w	r1, [r4, #93]	; 0x5d
  4245f0:	8142      	strh	r2, [r0, #10]
  4245f2:	1c48      	adds	r0, r1, #1
  4245f4:	f884 005d 	strb.w	r0, [r4, #93]	; 0x5d
  4245f8:	9806      	ldr	r0, [sp, #24]
  4245fa:	491a      	ldr	r1, [pc, #104]	; (424664
  4245fc:	4479      	add	r1, pc
  4245fe:	6809      	ldr	r1, [r1, #0]
  424600:	6809      	ldr	r1, [r1, #0]
  424602:	4281      	cmp	r1, r0
  424604:	bf02      	ittt	eq
  424606:	b007      	addeq	sp, #28
  424608:	e8bd 0f00 	ldmiaeq.w	sp!, {r8, r9, sl, fp}
  42460c:	bdf0      	popeq	{r4, r5, r6, r7, pc}
  42460e:	f09c e1e0 	blx	8c09d0 // __stack_chk_fail
  424612:	f1a4 0901 	sub.w	r9, r4, #1
  424616:	e006      	b.n	424626
  424618:	7a32      	ldrb	r2, [r6, #8]
  42461a:	b2cb      	uxtb	r3, r1
  42461c:	429a      	cmp	r2, r3
  42461e:	d019      	beq.n	424654
  424620:	6836      	ldr	r6, [r6, #0]
  424622:	2e00      	cmp	r6, #0
  424624:	d0cf      	beq.n	4245c6
  424626:	6872      	ldr	r2, [r6, #4]
  424628:	428a      	cmp	r2, r1
  42462a:	d0f5      	beq.n	424618
  42462c:	f1b8 0f01 	cmp.w	r8, #1
  424630:	d802      	bhi.n	424638
  424632:	ea02 0209 	and.w	r2, r2, r9
  424636:	e00a      	b.n	42464e
  424638:	42a2      	cmp	r2, r4
  42463a:	d308      	bcc.n	42464e
  42463c:	4605      	mov	r5, r0
  42463e:	468a      	mov	sl, r1
  424640:	4610      	mov	r0, r2
  424642:	4621      	mov	r1, r4
  424644:	f094 e1f0 	blx	8b8a28
  424648:	460a      	mov	r2, r1
  42464a:	4628      	mov	r0, r5
  42464c:	4651      	mov	r1, sl
  42464e:	455a      	cmp	r2, fp
  424650:	d0e6      	beq.n	424620
  424652:	e7b8      	b.n	4245c6
  424654:	8970      	ldrh	r0, [r6, #10]
  424656:	3001      	adds	r0, #1
  424658:	8170      	strh	r0, [r6, #10]
  42465a:	e7cd      	b.n	4245f8
  42465c:	b2ea      	uxtb	r2, r5
  42465e:	0051      	lsls	r1, r2, #1
  424660:	b266      	sxtb	r6, r4
  424662:	0051      	lsls	r1, r2, #1
  424664:	b21c      	sxth	r4, r3
  424666:	0051      	lsls	r1, r2, #1
