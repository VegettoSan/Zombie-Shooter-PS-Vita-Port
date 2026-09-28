  3f6060:	b5d0      	push	{r4, r6, r7, lr}
  3f6062:	af02      	add	r7, sp, #8
  3f6064:	b088      	sub	sp, #32
  3f6066:	4604      	mov	r4, r0
  3f6068:	481c      	ldr	r0, [pc, #112]	; (3f60dc
  3f606a:	2200      	movs	r2, #0
  3f606c:	4478      	add	r0, pc
  3f606e:	6800      	ldr	r0, [r0, #0]
  3f6070:	6800      	ldr	r0, [r0, #0]
  3f6072:	9007      	str	r0, [sp, #28]
  3f6074:	6808      	ldr	r0, [r1, #0]
  3f6076:	f88d 2018 	strb.w	r2, [sp, #24]
  3f607a:	f88d 200c 	strb.w	r2, [sp, #12]
  3f607e:	68c3      	ldr	r3, [r0, #12]
  3f6080:	aa03      	add	r2, sp, #12
  3f6082:	4668      	mov	r0, sp
  3f6084:	4798      	blx	r3
  3f6086:	466a      	mov	r2, sp
  3f6088:	4620      	mov	r0, r4
  3f608a:	f0ce e19a 	blx	8c43c0 // core::crypto::ICryptEngine::generateKey(STRING const&)
  3f608e:	4668      	mov	r0, sp
  3f6090:	f0ca e48e 	blx	8c09b0 // STRING::~STRING()
  3f6094:	f89d 0018 	ldrb.w	r0, [sp, #24]
  3f6098:	2800      	cmp	r0, #0
  3f609a:	bf1f      	itttt	ne
  3f609c:	9803      	ldrne	r0, [sp, #12]
  3f609e:	2800      	cmpne	r0, #0
  3f60a0:	9004      	strne	r0, [sp, #16]
  3f60a2:	f0ca e4a6 	blxne	8c09f0
  3f60a6:	9807      	ldr	r0, [sp, #28]
  3f60a8:	490d      	ldr	r1, [pc, #52]	; (3f60e0
  3f60aa:	4479      	add	r1, pc
  3f60ac:	6809      	ldr	r1, [r1, #0]
  3f60ae:	6809      	ldr	r1, [r1, #0]
  3f60b0:	4281      	cmp	r1, r0
  3f60b2:	bf04      	itt	eq
  3f60b4:	b008      	addeq	sp, #32
  3f60b6:	bdd0      	popeq	{r4, r6, r7, pc}
  3f60b8:	f0ca e48a 	blx	8c09d0 // __stack_chk_fail
  3f60bc:	4668      	mov	r0, sp
  3f60be:	f0ca e478 	blx	8c09b0 // STRING::~STRING()
  3f60c2:	e7ff      	b.n	3f60c4
  3f60c4:	f89d 0018 	ldrb.w	r0, [sp, #24]
  3f60c8:	2800      	cmp	r0, #0
  3f60ca:	bf1f      	itttt	ne
  3f60cc:	9803      	ldrne	r0, [sp, #12]
  3f60ce:	2800      	cmpne	r0, #0
  3f60d0:	9004      	strne	r0, [sp, #16]
  3f60d2:	f0ca e48e 	blxne	8c09f0
  3f60d6:	f0ca e484 	blx	8c09e0 // __cxa_end_cleanup
  3f60dc:	97ac      	str	r7, [sp, #688]	; 0x2b0
  3f60de:	0054      	lsls	r4, r2, #1
  3f60e0:	976e      	str	r7, [sp, #440]	; 0x1b8
  3f60e2:	0054      	lsls	r4, r2, #1
  3f60e4:	b5f0      	push	{r4, r5, r6, r7, lr}
  3f60e6:	af03      	add	r7, sp, #12
  3f60e8:	e92d 0f00 	stmdb	sp!, {r8, r9, sl, fp}
  3f60ec:	b08d      	sub	sp, #52	; 0x34
  3f60ee:	9001      	str	r0, [sp, #4]
  3f60f0:	2300      	movs	r3, #0
  3f60f2:	4861      	ldr	r0, [pc, #388]	; (3f6278
  3f60f4:	4478      	add	r0, pc
  3f60f6:	6800      	ldr	r0, [r0, #0]
  3f60f8:	6800      	ldr	r0, [r0, #0]
  3f60fa:	900c      	str	r0, [sp, #48]	; 0x30
  3f60fc:	7b10      	ldrb	r0, [r2, #12]
  3f60fe:	9308      	str	r3, [sp, #32]
  3f6100:	e9cd 3306 	strd	r3, r3, [sp, #24]
  3f6104:	b158      	cbz	r0, 3f611e
  3f6106:	a806      	add	r0, sp, #24
  3f6108:	4290      	cmp	r0, r2
  3f610a:	d05f      	beq.n	3f61cc
  3f610c:	e9d2 1200 	ldrd	r1, r2, [r2]
  3f6110:	1a53      	subs	r3, r2, r1
  3f6112:	a806      	add	r0, sp, #24
  3f6114:	f0ce e16c 	blx	8c43f0 // void std::__ndk1::vector<unsigned char, std::__ndk1::allocator<unsigned char> >::__assign_with_size[abi:ne190000]<unsigned char*, unsigned char*>(unsigned char*, unsigned char*, int)
  3f6118:	e9dd 5106 	ldrd	r5, r1, [sp, #24]
  3f611c:	e00f      	b.n	3f613e
  3f611e:	6808      	ldr	r0, [r1, #0]
  3f6120:	6902      	ldr	r2, [r0, #16]
  3f6122:	a809      	add	r0, sp, #36	; 0x24
  3f6124:	4790      	blx	r2
  3f6126:	9806      	ldr	r0, [sp, #24]
  3f6128:	2800      	cmp	r0, #0
  3f612a:	bf1c      	itt	ne
  3f612c:	9007      	strne	r0, [sp, #28]
  3f612e:	f0ca e460 	blxne	8c09f0
  3f6132:	e9dd 5109 	ldrd	r5, r1, [sp, #36]	; 0x24
  3f6136:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f6138:	e9cd 5106 	strd	r5, r1, [sp, #24]
  3f613c:	9008      	str	r0, [sp, #32]
  3f613e:	2000      	movs	r0, #0
  3f6140:	428d      	cmp	r5, r1
  3f6142:	900b      	str	r0, [sp, #44]	; 0x2c
  3f6144:	e9cd 0009 	strd	r0, r0, [sp, #36]	; 0x24
  3f6148:	d044      	beq.n	3f61d4
  3f614a:	f04f 0800 	mov.w	r8, #0
  3f614e:	9102      	str	r1, [sp, #8]
  3f6150:	e006      	b.n	3f6160
  3f6152:	f808 bb01 	strb.w	fp, [r8], #1
  3f6156:	3501      	adds	r5, #1
  3f6158:	f8cd 8028 	str.w	r8, [sp, #40]	; 0x28
  3f615c:	428d      	cmp	r5, r1
  3f615e:	d039      	beq.n	3f61d4
  3f6160:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f6162:	f895 b000 	ldrb.w	fp, [r5]
  3f6166:	4580      	cmp	r8, r0
  3f6168:	d3f3      	bcc.n	3f6152
  3f616a:	f8dd a024 	ldr.w	sl, [sp, #36]	; 0x24
  3f616e:	eba8 090a 	sub.w	r9, r8, sl
  3f6172:	f109 0601 	add.w	r6, r9, #1
  3f6176:	f1b6 3fff 	cmp.w	r6, #4294967295	; 0xffffffff
  3f617a:	dd64      	ble.n	3f6246
  3f617c:	eba0 000a 	sub.w	r0, r0, sl
  3f6180:	f06f 4140 	mvn.w	r1, #3221225472	; 0xc0000000
  3f6184:	ebb6 0f40 	cmp.w	r6, r0, lsl #1
  3f6188:	bf38      	it	cc
  3f618a:	0046      	lslcc	r6, r0, #1
  3f618c:	4288      	cmp	r0, r1
  3f618e:	bf28      	it	cs
  3f6190:	f06f 4600 	mvncs.w	r6, #2147483648	; 0x80000000
  3f6194:	b126      	cbz	r6, 3f61a0
  3f6196:	4630      	mov	r0, r6
  3f6198:	f0ca e3e2 	blx	8c0960 // operator new(unsigned int)
  3f619c:	4604      	mov	r4, r0
  3f619e:	e000      	b.n	3f61a2
  3f61a0:	2400      	movs	r4, #0
  3f61a2:	eb04 0809 	add.w	r8, r4, r9
  3f61a6:	4620      	mov	r0, r4
  3f61a8:	4651      	mov	r1, sl
  3f61aa:	464a      	mov	r2, r9
  3f61ac:	f808 bb01 	strb.w	fp, [r8], #1
  3f61b0:	f0c2 e428 	blx	8b8a04
  3f61b4:	f1ba 0f00 	cmp.w	sl, #0
  3f61b8:	eb04 0006 	add.w	r0, r4, r6
  3f61bc:	900b      	str	r0, [sp, #44]	; 0x2c
  3f61be:	9409      	str	r4, [sp, #36]	; 0x24
  3f61c0:	bf1c      	itt	ne
  3f61c2:	4650      	movne	r0, sl
  3f61c4:	f0ca e414 	blxne	8c09f0
  3f61c8:	9902      	ldr	r1, [sp, #8]
  3f61ca:	e7c4      	b.n	3f6156
  3f61cc:	2000      	movs	r0, #0
  3f61ce:	900b      	str	r0, [sp, #44]	; 0x2c
  3f61d0:	e9cd 0009 	strd	r0, r0, [sp, #36]	; 0x24
  3f61d4:	a909      	add	r1, sp, #36	; 0x24
  3f61d6:	2000      	movs	r0, #0
  3f61d8:	f000 f948 	bl	3f646c
  3f61dc:	a909      	add	r1, sp, #36	; 0x24
  3f61de:	2001      	movs	r0, #1
  3f61e0:	f000 f944 	bl	3f646c
  3f61e4:	a909      	add	r1, sp, #36	; 0x24
  3f61e6:	2002      	movs	r0, #2
  3f61e8:	f000 f940 	bl	3f646c
  3f61ec:	a909      	add	r1, sp, #36	; 0x24
  3f61ee:	2003      	movs	r0, #3
  3f61f0:	f000 f93c 	bl	3f646c
  3f61f4:	e9dd 1009 	ldrd	r1, r0, [sp, #36]	; 0x24
  3f61f8:	1a42      	subs	r2, r0, r1
  3f61fa:	a803      	add	r0, sp, #12
  3f61fc:	f0cb e3e8 	blx	8c19d0 // core::crypto::sha256(unsigned char const*, unsigned int)
  3f6200:	9809      	ldr	r0, [sp, #36]	; 0x24
  3f6202:	2800      	cmp	r0, #0
  3f6204:	bf1c      	itt	ne
  3f6206:	900a      	strne	r0, [sp, #40]	; 0x28
  3f6208:	f0ca e3f2 	blxne	8c09f0
  3f620c:	9801      	ldr	r0, [sp, #4]
  3f620e:	a903      	add	r1, sp, #12
  3f6210:	f0cb e3e6 	blx	8c19e0 // core::crypto::digestToString(std::__ndk1::vector<unsigned char, std::__ndk1::allocator<unsigned char> > const&)
  3f6214:	9803      	ldr	r0, [sp, #12]
  3f6216:	2800      	cmp	r0, #0
  3f6218:	bf1c      	itt	ne
  3f621a:	9004      	strne	r0, [sp, #16]
  3f621c:	f0ca e3e8 	blxne	8c09f0
  3f6220:	9806      	ldr	r0, [sp, #24]
  3f6222:	2800      	cmp	r0, #0
  3f6224:	bf1c      	itt	ne
  3f6226:	9007      	strne	r0, [sp, #28]
  3f6228:	f0ca e3e2 	blxne	8c09f0
  3f622c:	980c      	ldr	r0, [sp, #48]	; 0x30
  3f622e:	4913      	ldr	r1, [pc, #76]	; (3f627c
  3f6230:	4479      	add	r1, pc
  3f6232:	6809      	ldr	r1, [r1, #0]
  3f6234:	6809      	ldr	r1, [r1, #0]
  3f6236:	4281      	cmp	r1, r0
  3f6238:	bf02      	ittt	eq
  3f623a:	b00d      	addeq	sp, #52	; 0x34
  3f623c:	e8bd 0f00 	ldmiaeq.w	sp!, {r8, r9, sl, fp}
  3f6240:	bdf0      	popeq	{r4, r5, r6, r7, pc}
  3f6242:	f0ca e3c6 	blx	8c09d0 // __stack_chk_fail
  3f6246:	a809      	add	r0, sp, #36	; 0x24
  3f6248:	f7fc f838 	bl	3f22bc
  3f624c:	e00c      	b.n	3f6268
  3f624e:	e00b      	b.n	3f6268
  3f6250:	9803      	ldr	r0, [sp, #12]
  3f6252:	b148      	cbz	r0, 3f6268
  3f6254:	9004      	str	r0, [sp, #16]
  3f6256:	e005      	b.n	3f6264
  3f6258:	e001      	b.n	3f625e
  3f625a:	e000      	b.n	3f625e
  3f625c:	e7ff      	b.n	3f625e
  3f625e:	9809      	ldr	r0, [sp, #36]	; 0x24
  3f6260:	b110      	cbz	r0, 3f6268
  3f6262:	900a      	str	r0, [sp, #40]	; 0x28
  3f6264:	f0ca e3c4 	blx	8c09f0 // operator delete(void*)
  3f6268:	9806      	ldr	r0, [sp, #24]
  3f626a:	2800      	cmp	r0, #0
  3f626c:	bf1c      	itt	ne
  3f626e:	9007      	strne	r0, [sp, #28]
  3f6270:	f0ca e3be 	blxne	8c09f0
  3f6274:	f0ca e3b4 	blx	8c09e0 // __cxa_end_cleanup
  3f6278:	9724      	str	r7, [sp, #144]	; 0x90
  3f627a:	0054      	lsls	r4, r2, #1
  3f627c:	95e8      	str	r5, [sp, #928]	; 0x3a0
  3f627e:	0054      	lsls	r4, r2, #1
  3f6280:	b5b0      	push	{r4, r5, r7, lr}
  3f6282:	af02      	add	r7, sp, #8
  3f6284:	b08c      	sub	sp, #48	; 0x30
  3f6286:	4604      	mov	r4, r0
  3f6288:	4827      	ldr	r0, [pc, #156]	; (3f6328
  3f628a:	4615      	mov	r5, r2
  3f628c:	2200      	movs	r2, #0
  3f628e:	4478      	add	r0, pc
  3f6290:	6800      	ldr	r0, [r0, #0]
  3f6292:	6800      	ldr	r0, [r0, #0]
  3f6294:	900b      	str	r0, [sp, #44]	; 0x2c
  3f6296:	6808      	ldr	r0, [r1, #0]
  3f6298:	f88d 2028 	strb.w	r2, [sp, #40]	; 0x28
  3f629c:	f88d 201c 	strb.w	r2, [sp, #28]
  3f62a0:	68c3      	ldr	r3, [r0, #12]
  3f62a2:	a801      	add	r0, sp, #4
  3f62a4:	aa07      	add	r2, sp, #28
  3f62a6:	4798      	blx	r3
  3f62a8:	a804      	add	r0, sp, #16
  3f62aa:	a901      	add	r1, sp, #4
  3f62ac:	462a      	mov	r2, r5
  3f62ae:	f0cd e020 	blx	8c32f0 // STRING::operator+(STRING const&) const
  3f62b2:	a801      	add	r0, sp, #4
  3f62b4:	f0ca e37c 	blx	8c09b0 // STRING::~STRING()
  3f62b8:	f89d 0028 	ldrb.w	r0, [sp, #40]	; 0x28
  3f62bc:	2800      	cmp	r0, #0
  3f62be:	bf1f      	itttt	ne
  3f62c0:	9807      	ldrne	r0, [sp, #28]
  3f62c2:	2800      	cmpne	r0, #0
  3f62c4:	9008      	strne	r0, [sp, #32]
  3f62c6:	f0ca e394 	blxne	8c09f0
  3f62ca:	a804      	add	r0, sp, #16
  3f62cc:	f0cb e080 	blx	8c13d0 // STRING::CharPtr() const
  3f62d0:	4605      	mov	r5, r0
  3f62d2:	a804      	add	r0, sp, #16
  3f62d4:	f0cb e534 	blx	8c1d40 // STRING::Length() const
  3f62d8:	4602      	mov	r2, r0
  3f62da:	4620      	mov	r0, r4
  3f62dc:	4629      	mov	r1, r5
  3f62de:	f0cb e558 	blx	8c1d90 // core::crypto::md5(char const*, unsigned int)
  3f62e2:	a804      	add	r0, sp, #16
  3f62e4:	f0ca e364 	blx	8c09b0 // STRING::~STRING()
  3f62e8:	980b      	ldr	r0, [sp, #44]	; 0x2c
  3f62ea:	4910      	ldr	r1, [pc, #64]	; (3f632c
  3f62ec:	4479      	add	r1, pc
  3f62ee:	6809      	ldr	r1, [r1, #0]
  3f62f0:	6809      	ldr	r1, [r1, #0]
  3f62f2:	4281      	cmp	r1, r0
  3f62f4:	bf04      	itt	eq
  3f62f6:	b00c      	addeq	sp, #48	; 0x30
  3f62f8:	bdb0      	popeq	{r4, r5, r7, pc}
  3f62fa:	f0ca e36a 	blx	8c09d0 // __stack_chk_fail
  3f62fe:	a801      	add	r0, sp, #4
  3f6300:	f0ca e356 	blx	8c09b0 // STRING::~STRING()
  3f6304:	e7ff      	b.n	3f6306
  3f6306:	f89d 0028 	ldrb.w	r0, [sp, #40]	; 0x28
  3f630a:	2800      	cmp	r0, #0
  3f630c:	bf1c      	itt	ne
  3f630e:	9807      	ldrne	r0, [sp, #28]
  3f6310:	2800      	cmpne	r0, #0
  3f6312:	d007      	beq.n	3f6324
  3f6314:	9008      	str	r0, [sp, #32]
  3f6316:	f0ca e36c 	blx	8c09f0 // operator delete(void*)
  3f631a:	f0ca e362 	blx	8c09e0 // __cxa_end_cleanup
  3f631e:	a804      	add	r0, sp, #16
  3f6320:	f0ca e346 	blx	8c09b0 // STRING::~STRING()
  3f6324:	f0ca e35c 	blx	8c09e0 // __cxa_end_cleanup
  3f6328:	958a      	str	r5, [sp, #552]	; 0x228
  3f632a:	0054      	lsls	r4, r2, #1
  3f632c:	952c      	str	r5, [sp, #176]	; 0xb0
  3f632e:	0054      	lsls	r4, r2, #1
