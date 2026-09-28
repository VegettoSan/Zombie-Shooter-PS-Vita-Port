  3f7440:	b5f0      	push	{r4, r5, r6, r7, lr}
  3f7442:	af03      	add	r7, sp, #12
  3f7444:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  3f7448:	b0ad      	sub	sp, #180	; 0xb4
  3f744a:	4605      	mov	r5, r0
  3f744c:	f8df 04b4 	ldr.w	r0, [pc, #1204]	; 3f7904
  3f7450:	460c      	mov	r4, r1
  3f7452:	4478      	add	r0, pc
  3f7454:	6800      	ldr	r0, [r0, #0]
  3f7456:	6800      	ldr	r0, [r0, #0]
  3f7458:	902c      	str	r0, [sp, #176]	; 0xb0
  3f745a:	e9d1 0100 	ldrd	r0, r1, [r1]
  3f745e:	4288      	cmp	r0, r1
  3f7460:	f000 8083 	beq.w	3f756a
  3f7464:	e9d3 0100 	ldrd	r0, r1, [r3]
  3f7468:	1a08      	subs	r0, r1, r0
  3f746a:	2820      	cmp	r0, #32
  3f746c:	f040 80a9 	bne.w	3f75c2
  3f7470:	2010      	movs	r0, #16
  3f7472:	4690      	mov	r8, r2
  3f7474:	4699      	mov	r9, r3
  3f7476:	f0c9 e274 	blx	8c0960 // operator new(unsigned int)
  3f747a:	682e      	ldr	r6, [r5, #0]
  3f747c:	4682      	mov	sl, r0
  3f747e:	f04f 30a0 	mov.w	r0, #2694881440	; 0xa0a0a0a0
  3f7482:	f8ca 0000 	str.w	r0, [sl]
  3f7486:	f8ca 0004 	str.w	r0, [sl, #4]
  3f748a:	f8ca 0008 	str.w	r0, [sl, #8]
  3f748e:	f8ca 000c 	str.w	r0, [sl, #12]
  3f7492:	f0cd e686 	blx	8c51a0 // EVP_aes_256_cbc
  3f7496:	4601      	mov	r1, r0
  3f7498:	2001      	movs	r0, #1
  3f749a:	2200      	movs	r2, #0
  3f749c:	e9cd 2000 	strd	r2, r0, [sp]
  3f74a0:	4630      	mov	r0, r6
  3f74a2:	2300      	movs	r3, #0
  3f74a4:	f0cd e654 	blx	8c5150 // EVP_CipherInit_ex
  3f74a8:	6828      	ldr	r0, [r5, #0]
  3f74aa:	f0cd e65a 	blx	8c5160 // EVP_CIPHER_CTX_get_key_length
  3f74ae:	2820      	cmp	r0, #32
  3f74b0:	f040 80b7 	bne.w	3f7622
  3f74b4:	6828      	ldr	r0, [r5, #0]
  3f74b6:	f0cd e65c 	blx	8c5170 // EVP_CIPHER_CTX_get_iv_length
  3f74ba:	2810      	cmp	r0, #16
  3f74bc:	f040 80df 	bne.w	3f767e
  3f74c0:	6828      	ldr	r0, [r5, #0]
  3f74c2:	f8d9 3000 	ldr.w	r3, [r9]
  3f74c6:	2101      	movs	r1, #1
  3f74c8:	2200      	movs	r2, #0
  3f74ca:	e9cd a100 	strd	sl, r1, [sp]
  3f74ce:	2100      	movs	r1, #0
  3f74d0:	f0cd e63e 	blx	8c5150 // EVP_CipherInit_ex
  3f74d4:	2800      	cmp	r0, #0
  3f74d6:	f000 8100 	beq.w	3f76da
  3f74da:	e9d4 0100 	ldrd	r0, r1, [r4]
  3f74de:	e9d8 9b00 	ldrd	r9, fp, [r8]
  3f74e2:	1a08      	subs	r0, r1, r0
  3f74e4:	f8cd a010 	str.w	sl, [sp, #16]
  3f74e8:	f020 000f 	bic.w	r0, r0, #15
  3f74ec:	ebab 0a09 	sub.w	sl, fp, r9
  3f74f0:	f100 0610 	add.w	r6, r0, #16
  3f74f4:	4556      	cmp	r6, sl
  3f74f6:	f240 8127 	bls.w	3f7748
  3f74fa:	f8d8 0008 	ldr.w	r0, [r8, #8]
  3f74fe:	eba6 010a 	sub.w	r1, r6, sl
  3f7502:	eba0 020b 	sub.w	r2, r0, fp
  3f7506:	428a      	cmp	r2, r1
  3f7508:	f080 8122 	bcs.w	3f7750
  3f750c:	f1b6 3fff 	cmp.w	r6, #4294967295	; 0xffffffff
  3f7510:	9103      	str	r1, [sp, #12]
  3f7512:	f340 81cb 	ble.w	3f78ac
  3f7516:	eba0 0209 	sub.w	r2, r0, r9
  3f751a:	4630      	mov	r0, r6
  3f751c:	f06f 4140 	mvn.w	r1, #3221225472	; 0xc0000000
  3f7520:	ebb6 0f42 	cmp.w	r6, r2, lsl #1
  3f7524:	bf38      	it	cc
  3f7526:	0050      	lslcc	r0, r2, #1
  3f7528:	428a      	cmp	r2, r1
  3f752a:	bf28      	it	cs
  3f752c:	f06f 4000 	mvncs.w	r0, #2147483648	; 0x80000000
  3f7530:	9002      	str	r0, [sp, #8]
  3f7532:	f0c9 e216 	blx	8c0960 // operator new(unsigned int)
  3f7536:	9903      	ldr	r1, [sp, #12]
  3f7538:	4683      	mov	fp, r0
  3f753a:	4450      	add	r0, sl
  3f753c:	f0c1 e26e 	blx	8b8a1c
  3f7540:	4658      	mov	r0, fp
  3f7542:	4649      	mov	r1, r9
  3f7544:	4652      	mov	r2, sl
  3f7546:	f0c1 e25e 	blx	8b8a04
  3f754a:	9802      	ldr	r0, [sp, #8]
  3f754c:	eb0b 0106 	add.w	r1, fp, r6
  3f7550:	f1b9 0f00 	cmp.w	r9, #0
  3f7554:	4458      	add	r0, fp
  3f7556:	e9c8 b100 	strd	fp, r1, [r8]
  3f755a:	f8c8 0008 	str.w	r0, [r8, #8]
  3f755e:	f000 80ff 	beq.w	3f7760
  3f7562:	4648      	mov	r0, r9
  3f7564:	f0c9 e244 	blx	8c09f0 // operator delete(void*)
  3f7568:	e0fa      	b.n	3f7760
  3f756a:	f0c9 e5aa 	blx	8c10c0 // core::Log::reportingLevel()
  3f756e:	2802      	cmp	r0, #2
  3f7570:	d355      	bcc.n	3f761e
  3f7572:	a809      	add	r0, sp, #36	; 0x24
  3f7574:	2102      	movs	r1, #2
  3f7576:	f0c9 e5ac 	blx	8c10d0 // core::Log::Log(core::ILogger::Level)
  3f757a:	f0c9 e5b2 	blx	8c10e0 // core::Log::get()
  3f757e:	49e2      	ldr	r1, [pc, #904]	; (3f7908
  3f7580:	4479      	add	r1, pc // @2ce664 "[AES] Empty data"
  3f7582:	2210      	movs	r2, #16
  3f7584:	f7ce faba 	bl	3c5afc
  3f7588:	49e0      	ldr	r1, [pc, #896]	; (3f790c
  3f758a:	4479      	add	r1, pc // @312de5 " in '"
  3f758c:	2205      	movs	r2, #5
  3f758e:	f7ce fab5 	bl	3c5afc
  3f7592:	49df      	ldr	r1, [pc, #892]	; (3f7910
  3f7594:	4479      	add	r1, pc // @2d7e36 "bool core::crypto::Chipher<1, 256>::porcess(const Bytes &, Bytes &, const Bytes &) [EncryptOrDecrypt = 1, KEY_LEN = 256]"
  3f7596:	2278      	movs	r2, #120	; 0x78
  3f7598:	f7ce fab0 	bl	3c5afc
  3f759c:	49dd      	ldr	r1, [pc, #884]	; (3f7914
  3f759e:	4479      	add	r1, pc // @2de008 "' <"
  3f75a0:	2203      	movs	r2, #3
  3f75a2:	f7ce faab 	bl	3c5afc
  3f75a6:	49dc      	ldr	r1, [pc, #880]	; (3f7918
  3f75a8:	4479      	add	r1, pc // @2fb9dd "D:/repository/sources/core/crypto/aes256.cpp"
  3f75aa:	222c      	movs	r2, #44	; 0x2c
  3f75ac:	f7ce faa6 	bl	3c5afc
  3f75b0:	49da      	ldr	r1, [pc, #872]	; (3f791c
  3f75b2:	4479      	add	r1, pc // @2e14ac "> at "
  3f75b4:	2205      	movs	r2, #5
  3f75b6:	f7ce faa1 	bl	3c5afc
  3f75ba:	2183      	movs	r1, #131	; 0x83
  3f75bc:	f0c9 e688 	blx	8c12d0 // std::__ndk1::basic_ostream<char, std::__ndk1::char_traits<char> >::operator<<(int)
  3f75c0:	e02a      	b.n	3f7618
  3f75c2:	f0c9 e57e 	blx	8c10c0 // core::Log::reportingLevel()
  3f75c6:	2802      	cmp	r0, #2
  3f75c8:	d329      	bcc.n	3f761e
  3f75ca:	a809      	add	r0, sp, #36	; 0x24
  3f75cc:	2102      	movs	r1, #2
  3f75ce:	f0c9 e580 	blx	8c10d0 // core::Log::Log(core::ILogger::Level)
  3f75d2:	f0c9 e586 	blx	8c10e0 // core::Log::get()
  3f75d6:	49d2      	ldr	r1, [pc, #840]	; (3f7920
  3f75d8:	4479      	add	r1, pc // @2eb5fb "[AES] Invalid key length"
  3f75da:	2218      	movs	r2, #24
  3f75dc:	f7ce fa8e 	bl	3c5afc
  3f75e0:	49d0      	ldr	r1, [pc, #832]	; (3f7924
  3f75e2:	4479      	add	r1, pc // @312de5 " in '"
  3f75e4:	2205      	movs	r2, #5
  3f75e6:	f7ce fa89 	bl	3c5afc
  3f75ea:	49cf      	ldr	r1, [pc, #828]	; (3f7928
  3f75ec:	4479      	add	r1, pc // @2d7e36 "bool core::crypto::Chipher<1, 256>::porcess(const Bytes &, Bytes &, const Bytes &) [EncryptOrDecrypt = 1, KEY_LEN = 256]"
  3f75ee:	2278      	movs	r2, #120	; 0x78
  3f75f0:	f7ce fa84 	bl	3c5afc
  3f75f4:	49cd      	ldr	r1, [pc, #820]	; (3f792c
  3f75f6:	4479      	add	r1, pc // @2de008 "' <"
  3f75f8:	2203      	movs	r2, #3
  3f75fa:	f7ce fa7f 	bl	3c5afc
  3f75fe:	49cc      	ldr	r1, [pc, #816]	; (3f7930
  3f7600:	4479      	add	r1, pc // @2fb9dd "D:/repository/sources/core/crypto/aes256.cpp"
  3f7602:	222c      	movs	r2, #44	; 0x2c
  3f7604:	f7ce fa7a 	bl	3c5afc
  3f7608:	49ca      	ldr	r1, [pc, #808]	; (3f7934
  3f760a:	4479      	add	r1, pc // @2e14ac "> at "
  3f760c:	2205      	movs	r2, #5
  3f760e:	f7ce fa75 	bl	3c5afc
  3f7612:	2189      	movs	r1, #137	; 0x89
  3f7614:	f0c9 e65c 	blx	8c12d0 // std::__ndk1::basic_ostream<char, std::__ndk1::char_traits<char> >::operator<<(int)
  3f7618:	a809      	add	r0, sp, #36	; 0x24
  3f761a:	f0c9 e572 	blx	8c1100 // core::Log::~Log()
  3f761e:	2000      	movs	r0, #0
  3f7620:	e137      	b.n	3f7892
  3f7622:	f0c9 e54e 	blx	8c10c0 // core::Log::reportingLevel()
  3f7626:	2802      	cmp	r0, #2
  3f7628:	f0c0 812e 	bcc.w	3f7888
  3f762c:	a809      	add	r0, sp, #36	; 0x24
  3f762e:	2102      	movs	r1, #2
