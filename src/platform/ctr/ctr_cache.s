.arm

@ ARM11 MPCore: 32-byte cache lines.
@ Entire-cache flush is far too expensive for dynrec block close;
@ flush only the translated range instead.

.global _InvalidateAndFlushCaches
.global _InvalidateAndFlushCachesRange

@ Arguments for the range kernel stub (set from C before svcCustomBackdoor).
.global ctr_cache_flush_start
.global ctr_cache_flush_size

.bss
.align 2
ctr_cache_flush_start:
	.space 4
ctr_cache_flush_size:
	.space 4

.text

@--------------------------------------------------------------------
@ Kernel: clean+invalidate D-cache + invalidate I-cache for [start, start+size)
@--------------------------------------------------------------------
_FlushRangeKernel:
	cpsid aif
	ldr r0, =ctr_cache_flush_start
	ldr r0, [r0]
	ldr r1, =ctr_cache_flush_size
	ldr r1, [r1]
	cmp r1, #0
	beq 2f

	add r1, r0, r1			@ end
	bic r0, r0, #0x1f		@ align start down to 32B

1:
	mcr p15, 0, r0, c7, c14, 1	@ clean+invalidate D line by MVA
	mcr p15, 0, r0, c7, c5, 1	@ invalidate I line by MVA
	add r0, r0, #0x20
	cmp r0, r1
	blo 1b

2:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4	@ DSB
	mcr p15, 0, r0, c7, c5, 4	@ prefetch flush
	mcr p15, 0, r0, c7, c5, 6	@ flush BTB
	bx lr

@--------------------------------------------------------------------
@ Legacy whole-cache path (kept for rare callers)
@--------------------------------------------------------------------
_ClearCacheKernel:
	cpsid aif
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 0	@ Clean entire data cache
	mcr p15, 0, r0, c7, c10, 5	@ Data Memory Barrier
	mcr p15, 0, r0, c7, c5, 0	@ Invalidate entire instruction cache / Flush BTB
	mcr p15, 0, r0, c7, c10, 4	@ Data Sync Barrier
	bx lr

_InvalidateAndFlushCaches:
	ldr r0, =_ClearCacheKernel
	svc 0x80					@ svcCustomBackdoor
	bx lr

@ void _InvalidateAndFlushCachesRange(void *start, unsigned size)
@ r0=start, r1=size
_InvalidateAndFlushCachesRange:
	ldr r2, =ctr_cache_flush_start
	str r0, [r2]
	ldr r2, =ctr_cache_flush_size
	str r1, [r2]
	ldr r0, =_FlushRangeKernel
	svc 0x80					@ svcCustomBackdoor
	bx lr
