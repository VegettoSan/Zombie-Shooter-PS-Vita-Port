  3f850c:	b5f0      	push	{r4, r5, r6, r7, lr}
  3f850e:	af03      	add	r7, sp, #12
  3f8510:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  3f8514:	b08b      	sub	sp, #44	; 0x2c
  3f8516:	4683      	mov	fp, r0
  3f8518:	487f      	ldr	r0, [pc, #508]	; (3f8718
  3f851a:	4980      	ldr	r1, [pc, #512]	; (3f871c
  3f851c:	4478      	add	r0, pc
  3f851e:	4479      	add	r1, pc
  3f8520:	6809      	ldr	r1, [r1, #0]
  3f8522:	6809      	ldr	r1, [r1, #0]
  3f8524:	910a      	str	r1, [sp, #40]	; 0x28
  3f8526:	7800      	ldrb	r0, [r0, #0]
  3f8528:	f3bf 8f5b 	dmb	ish
  3f852c:	4c7c      	ldr	r4, [pc, #496]	; (3f8720
  3f852e:	447c      	add	r4, pc
  3f8530:	07c0      	lsls	r0, r0, #31
  3f8532:	f000 80b9 	beq.w	3f86a8
  3f8536:	e9d4 6000 	ldrd	r6, r0, [r4]
  3f853a:	42b0      	cmp	r0, r6
  3f853c:	f040 808e 	bne.w	3f865c
  3f8540:	a808      	add	r0, sp, #32
  3f8542:	2180      	movs	r1, #128	; 0x80
  3f8544:	f0c8 e64c 	blx	8c11e0 // jnipp::Environment::Environment(unsigned int)
  3f8548:	f0c8 e652 	blx	8c11f0 // android::ApplicationNative::nativeApp()
  3f854c:	f0c8 e658 	blx	8c1200 // android::ApplicationNative::getMainActivity() const
  3f8550:	4681      	mov	r9, r0
  3f8552:	a808      	add	r0, sp, #32
  3f8554:	f0c8 e674 	blx	8c1240 // jnipp::Environment::operator->()
  3f8558:	6801      	ldr	r1, [r0, #0]
  3f855a:	6fca      	ldr	r2, [r1, #124]	; 0x7c
  3f855c:	4649      	mov	r1, r9
  3f855e:	4790      	blx	r2
  3f8560:	4606      	mov	r6, r0
  3f8562:	a808      	add	r0, sp, #32
  3f8564:	f0c8 e674 	blx	8c1250 // jnipp::Environment::checkException()
  3f8568:	a808      	add	r0, sp, #32
  3f856a:	f0c8 e66a 	blx	8c1240 // jnipp::Environment::operator->()
  3f856e:	6801      	ldr	r1, [r0, #0]
  3f8570:	f8d1 5084 	ldr.w	r5, [r1, #132]	; 0x84
  3f8574:	4a6f      	ldr	r2, [pc, #444]	; (3f8734
  3f8576:	4b70      	ldr	r3, [pc, #448]	; (3f8738
  3f8578:	447a      	add	r2, pc // @2d4e3a "getContentResolver"
  3f857a:	447b      	add	r3, pc // @3198be "()Landroid/content/ContentResolver;"
  3f857c:	4631      	mov	r1, r6
  3f857e:	47a8      	blx	r5
  3f8580:	4606      	mov	r6, r0
  3f8582:	a808      	add	r0, sp, #32
  3f8584:	f0c8 e664 	blx	8c1250 // jnipp::Environment::checkException()
  3f8588:	a808      	add	r0, sp, #32
  3f858a:	f0c8 e65a 	blx	8c1240 // jnipp::Environment::operator->()
  3f858e:	4649      	mov	r1, r9
  3f8590:	4632      	mov	r2, r6
  3f8592:	f0c8 e79e 	blx	8c14d0 // _JNIEnv::CallObjectMethod(_jobject*, _jmethodID*, ...)
  3f8596:	4680      	mov	r8, r0
  3f8598:	a808      	add	r0, sp, #32
  3f859a:	f0c8 e65a 	blx	8c1250 // jnipp::Environment::checkException()
  3f859e:	4967      	ldr	r1, [pc, #412]	; (3f873c
  3f85a0:	4479      	add	r1, pc // @2c8410 "android.provider.Settings$Secure"
  3f85a2:	a805      	add	r0, sp, #20
  3f85a4:	f0c8 e23c 	blx	8c0a20 // STRING::STRING(char const*)
  3f85a8:	a808      	add	r0, sp, #32
  3f85aa:	aa05      	add	r2, sp, #20
  3f85ac:	4649      	mov	r1, r9
  3f85ae:	f0c8 e630 	blx	8c1210 // jnipp::Environment::findClass(_jobject*, STRING const&)
  3f85b2:	4681      	mov	r9, r0
  3f85b4:	a805      	add	r0, sp, #20
  3f85b6:	f0c8 e1fc 	blx	8c09b0 // STRING::~STRING()
  3f85ba:	a808      	add	r0, sp, #32
  3f85bc:	f0c8 e648 	blx	8c1250 // jnipp::Environment::checkException()
  3f85c0:	a808      	add	r0, sp, #32
  3f85c2:	f0c8 e63e 	blx	8c1240 // jnipp::Environment::operator->()
  3f85c6:	6801      	ldr	r1, [r0, #0]
  3f85c8:	f8d1 61c4 	ldr.w	r6, [r1, #452]	; 0x1c4
  3f85cc:	4a5c      	ldr	r2, [pc, #368]	; (3f8740
  3f85ce:	4b5d      	ldr	r3, [pc, #372]	; (3f8744
  3f85d0:	447a      	add	r2, pc // @2ce65a "getString"
  3f85d2:	447b      	add	r3, pc // @2be169 "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;"
  3f85d4:	4649      	mov	r1, r9
  3f85d6:	47b0      	blx	r6
  3f85d8:	4682      	mov	sl, r0
  3f85da:	a808      	add	r0, sp, #32
  3f85dc:	f0c8 e638 	blx	8c1250 // jnipp::Environment::checkException()
  3f85e0:	4959      	ldr	r1, [pc, #356]	; (3f8748
  3f85e2:	4479      	add	r1, pc // @2eb614 "android_id"
  3f85e4:	a808      	add	r0, sp, #32
  3f85e6:	220a      	movs	r2, #10
  3f85e8:	f0ca e102 	blx	8c27f0 // jnipp::Environment::createString(std::__ndk1::basic_string_view<char, std::__ndk1::char_traits<char> >)
  3f85ec:	4606      	mov	r6, r0
  3f85ee:	a808      	add	r0, sp, #32
  3f85f0:	f0c8 e626 	blx	8c1240 // jnipp::Environment::operator->()
  3f85f4:	4649      	mov	r1, r9
  3f85f6:	4652      	mov	r2, sl
  3f85f8:	4643      	mov	r3, r8
  3f85fa:	9600      	str	r6, [sp, #0]
  3f85fc:	f0c9 e258 	blx	8c1ab0 // _JNIEnv::CallStaticObjectMethod(_jclass*, _jmethodID*, ...)
  3f8600:	9002      	str	r0, [sp, #8]
  3f8602:	a805      	add	r0, sp, #20
  3f8604:	a908      	add	r1, sp, #32
  3f8606:	aa02      	add	r2, sp, #8
  3f8608:	f0c8 e65a 	blx	8c12c0 // jnipp::Environment::string(_jstring* const&)
  3f860c:	a808      	add	r0, sp, #32
  3f860e:	f0c8 e618 	blx	8c1240 // jnipp::Environment::operator->()
  3f8612:	6801      	ldr	r1, [r0, #0]
  3f8614:	6dca      	ldr	r2, [r1, #92]	; 0x5c
  3f8616:	4631      	mov	r1, r6
  3f8618:	4790      	blx	r2
  3f861a:	a805      	add	r0, sp, #20
  3f861c:	f0c8 e1b8 	blx	8c0990 // STRING::c_str() const
  3f8620:	4606      	mov	r6, r0
  3f8622:	a805      	add	r0, sp, #20
  3f8624:	f0c9 e38c 	blx	8c1d40 // STRING::Length() const
  3f8628:	4602      	mov	r2, r0
  3f862a:	a802      	add	r0, sp, #8
  3f862c:	4631      	mov	r1, r6
  3f862e:	f0c9 e1d0 	blx	8c19d0 // core::crypto::sha256(unsigned char const*, unsigned int)
  3f8632:	6820      	ldr	r0, [r4, #0]
  3f8634:	b130      	cbz	r0, 3f8644
  3f8636:	6060      	str	r0, [r4, #4]
  3f8638:	f0c8 e1da 	blx	8c09f0 // operator delete(void*)
  3f863c:	2000      	movs	r0, #0
  3f863e:	e9c4 0000 	strd	r0, r0, [r4]
  3f8642:	60a0      	str	r0, [r4, #8]
  3f8644:	aa02      	add	r2, sp, #8
  3f8646:	ca07      	ldmia	r2, {r0, r1, r2}
  3f8648:	e884 0007 	stmia.w	r4, {r0, r1, r2}
  3f864c:	a805      	add	r0, sp, #20
  3f864e:	f0c8 e1b0 	blx	8c09b0 // STRING::~STRING()
  3f8652:	a808      	add	r0, sp, #32
  3f8654:	f0c8 e61c 	blx	8c1290 // jnipp::Environment::~Environment()
  3f8658:	e9d4 6000 	ldrd	r6, r0, [r4]
  3f865c:	2100      	movs	r1, #0
  3f865e:	42b0      	cmp	r0, r6
  3f8660:	e9cb 1100 	strd	r1, r1, [fp]
  3f8664:	f8cb 1008 	str.w	r1, [fp, #8]
  3f8668:	d011      	beq.n	3f868e
  3f866a:	1b85      	subs	r5, r0, r6
  3f866c:	f1b5 3fff 	cmp.w	r5, #4294967295	; 0xffffffff
  3f8670:	dd31      	ble.n	3f86d6
  3f8672:	4628      	mov	r0, r5
  3f8674:	f0c8 e174 	blx	8c0960 // operator new(unsigned int)
  3f8678:	4631      	mov	r1, r6
  3f867a:	462a      	mov	r2, r5
  3f867c:	1944      	adds	r4, r0, r5
  3f867e:	f8cb 4008 	str.w	r4, [fp, #8]
  3f8682:	f8cb 0000 	str.w	r0, [fp]
  3f8686:	f0c0 e1be 	blx	8b8a04
  3f868a:	f8cb 4004 	str.w	r4, [fp, #4]
  3f868e:	980a      	ldr	r0, [sp, #40]	; 0x28
  3f8690:	492e      	ldr	r1, [pc, #184]	; (3f874c
  3f8692:	4479      	add	r1, pc
  3f8694:	6809      	ldr	r1, [r1, #0]
  3f8696:	6809      	ldr	r1, [r1, #0]
  3f8698:	4281      	cmp	r1, r0
  3f869a:	bf02      	ittt	eq
  3f869c:	b00b      	addeq	sp, #44	; 0x2c
  3f869e:	e8bd 0f00 	ldmiaeq.w	sp!, {r8, r9, sl, fp}
  3f86a2:	bdf0      	popeq	{r4, r5, r6, r7, pc}
  3f86a4:	f0c8 e194 	blx	8c09d0 // __stack_chk_fail
  3f86a8:	481e      	ldr	r0, [pc, #120]	; (3f8724
  3f86aa:	4478      	add	r0, pc
  3f86ac:	f0c8 e330 	blx	8c0d10 // __cxa_guard_acquire
  3f86b0:	2800      	cmp	r0, #0
  3f86b2:	f43f af40 	beq.w	3f8536
  3f86b6:	481c      	ldr	r0, [pc, #112]	; (3f8728
  3f86b8:	2100      	movs	r1, #0
  3f86ba:	4a1c      	ldr	r2, [pc, #112]	; (3f872c
  3f86bc:	4478      	add	r0, pc // @3f8751 "h"
  3f86be:	e9c4 1100 	strd	r1, r1, [r4]
  3f86c2:	447a      	add	r2, pc
  3f86c4:	60a1      	str	r1, [r4, #8]
  3f86c6:	4621      	mov	r1, r4
  3f86c8:	f0c8 e122 	blx	8c0910 // __cxa_atexit
  3f86cc:	4818      	ldr	r0, [pc, #96]	; (3f8730
  3f86ce:	4478      	add	r0, pc
  3f86d0:	f0c8 e326 	blx	8c0d20 // __cxa_guard_release
  3f86d4:	e72f      	b.n	3f8536
  3f86d6:	4658      	mov	r0, fp
  3f86d8:	f7f9 fdf0 	bl	3f22bc
  3f86dc:	e00c      	b.n	3f86f8
  3f86de:	e00b      	b.n	3f86f8
  3f86e0:	e002      	b.n	3f86e8
  3f86e2:	e009      	b.n	3f86f8
  3f86e4:	e000      	b.n	3f86e8
  3f86e6:	e007      	b.n	3f86f8
  3f86e8:	a805      	add	r0, sp, #20
  3f86ea:	f0c8 e162 	blx	8c09b0 // STRING::~STRING()
  3f86ee:	e003      	b.n	3f86f8
  3f86f0:	e002      	b.n	3f86f8
  3f86f2:	e001      	b.n	3f86f8
  3f86f4:	e000      	b.n	3f86f8
  3f86f6:	e7ff      	b.n	3f86f8
  3f86f8:	a808      	add	r0, sp, #32
  3f86fa:	f0c8 e5ca 	blx	8c1290 // jnipp::Environment::~Environment()
  3f86fe:	f0c8 e170 	blx	8c09e0 // __cxa_end_cleanup
  3f8702:	f8db 0000 	ldr.w	r0, [fp]
  3f8706:	2800      	cmp	r0, #0
  3f8708:	d0f9      	beq.n	3f86fe
  3f870a:	f8cb 0004 	str.w	r0, [fp, #4]
  3f870e:	f0c8 e170 	blx	8c09f0 // operator delete(void*)
  3f8712:	f0c8 e166 	blx	8c09e0 // __cxa_end_cleanup
  3f8718:	b8a8      			;
  3f871a:	0055      	lsls	r5, r2, #1
  3f871c:	72fa      	strb	r2, [r7, #11]
  3f871e:	0054      	lsls	r4, r2, #1
  3f8720:	b88a      			;
  3f8722:	0055      	lsls	r5, r2, #1
  3f8724:	b71a      			;
  3f8726:	0055      	lsls	r5, r2, #1
  3f8728:	0091      	lsls	r1, r2, #2
  3f872a:	0000      	movs	r0, r0
  3f872c:	460a      	mov	r2, r1
  3f872e:	004f      	lsls	r7, r1, #1
  3f8730:	b6f6      			;
  3f8732:	0055      	lsls	r5, r2, #1
  3f8734:	c8be      	ldmia	r0!, {r1, r2, r3, r4, r5, r7}
  3f8736:	ffed 1340 			;
  3f873a:	fff2 fe6c 	vqrdmlah.s<illegal width 64>	<illegal reg q15.5>, q1, d28[0]
  3f873e:	ffec 6086 	vaddl.u32	q11, d28, d6
  3f8742:	ffed 5b93 			;
  3f8746:	ffec 302e 	vaddl.u32	<illegal reg q9.5>, d12, d30
  3f874a:	ffef 7186 	vaddw.u32	<illegal reg q11.5>,
  3f874e:	0054      	lsls	r4, r2, #1
