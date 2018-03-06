#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/addrspace.h>
	
	.text
	.set	noreorder
	.globl 	head_start
	.globl  memcpy
	.globl  memset

/* s0: address of 1 we are actully running at
 * s1: address of 1 linked time
 */
head_start:
#if defined(CONFIG_64BIT) && defined(DEBUG)
	/* Print A to serial */
	li	v1,'A'
	lui	v0,0xfff0
	ori	v0,v0,0x23
	dsll32	v0,v0,0xb
	ori	v0,v0,0x840
	sd	v1,0(v0)
#endif

	bgezal	zero, 1f	/* why not just bal ? */
	 nop
1:	move 	s0, ra
#ifdef CONFIG_64BIT
	dla	s1, 1b
#else
	la	s1, 1b
#endif

	/* save the arguments aside until the bss is done clearing */
	move	s3, a0
	move	s4, a1
	move	s5, a2
	move	s6, a3

	move    s2, s0
	subu	s2, s1
	beq	s2, zero, 2f
	 nop
	
	/* relocate ourself if needed*/

	/* XXX we should actually flush the caches after relocation
	 * (D-wb and I-inv).
	 * However relocation to the KSEG1 address is probably sufficient,
	 * as we don't expect the Icache or Dcache to be populated (valid) for
	 * the linked addresses
	 */
#ifdef CONFIG_64BIT
	dla	a0, head_end
	addu	a1, a0, s2
	dla	a2, _rofs_start
	dli	t0, CKSEG1
#else
	la	a0, head_end	/* a0: Linked head_end */
	addu	a1, a0, s2	/* a1: Real (runtime) head_end */
	la	a2, _etext	/* a2: Linked _etext */
	li	t0, KSEG1
#endif
	subu	a2, a0		/* a2: length */
	li	t1, 0x1fffffff
	and	a0, t1		/* a0: Physical Linked head_end */
	/* memcpy(KSEG1(Linked head_end), Real head_end, length) */
	bal     memcpy
	or	a0, t0

2:
	/* setup stack */
#ifdef CONFIG_64BIT
#if 0
/* XXX: for some reason the stack doesn't work. But the stack pointer we get
 * from the U-Boot works fine :)
 */
	dla	t0, stack_start
	lw	sp, (t0)
#endif
#else
	la	t0, stack_start
	lw	sp, (t0)
	subu    sp, 16
#endif

        /* zero out bss */
#ifdef CONFIG_64BIT
	dla	a0, _bss_start
	move	a1, zero
	dla	a2, _bss_end
#else
	la	a0, _bss_start
	move	a1, zero
	la	a2, _bss_end
#endif
	bal	memset
	 subu	a2, a0

	/* save input arguments */
	LONG_S	s3, boot_flags
        LONG_S	s4, rg_param0		# save rgloader arguments on ram
	LONG_S	s5, rg_param1
	LONG_S	s6, board_type 

#ifdef CONFIG_64BIT
	/* jump to do_boot(s0=real_head_start, s1=compiled_head_start, a3=?) */
        move	a0, s0
	move    a2, a3
	dla	t0, do_boot
#else
	/* jump to do_boot(s0=real_head_start, s1=compiled_head_start) */
        move	a0, s0
	la	t0, do_boot
#endif
	jalr	t0
	  move	a1, s1

	/* do_boot() should execute the kernel. We should never be here... */
#ifdef CONFIG_64BIT
	dla	t0, serial_putc
	jalr	t0
	 li	a0, 'X'
#else
	la	t0, serial_putc
	jalr	t0
	 li	a0, 0x59
#endif
head_end:
	b	head_end

memcpy:
	move	a3, a0
	beqz	a2, memcpy_end
	addiu	v1, a2, -1
	li	a2, -1
memcpy_loop:
	lbu	v0, 0(a1)
	addiu	v1, v1, -1
	addiu	a1, a1, 1
	sb	v0, 0(a0)
	bne	v1, a2, memcpy_loop
	addiu	a0, a0, 1
memcpy_end:
	jr	ra
	move	v0, a3

memset:
	move	v0, a0
	beqz	a2, memset_end
	addiu	v1, a2, -1
	li	a2, -1
memset_loop:
	addiu	v1, v1, -1
	sb	a1, 0(a0)
	bne	v1, a2, memset_loop
	addiu	a0, a0, 1
memset_end:
	jr	ra
	nop
