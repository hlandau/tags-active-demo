#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdnoreturn.h>

/* PowerPC AS - Tags Active Demonstration Program
 * for POWER9 PowerNV Systems (Talos, Blackbird, etc.) {{{1
 * ===================================================
 */

#define NL "\n"

/* Hypercalls {{{2
 * ==========
 */ 

/* Definitions {{{3
 * -----------
 */

// Hypercalls
#define H_GET_TERM_CHAR		0x54
#define H_PUT_TERM_CHAR		0x58

// Hypercall Return Codes
#define H_SUCCESS		0
#define H_BUSY			1

typedef int64_t HResult;

/* HPutTermChar {{{3
 * ------------
 * Hypercall to write character to console.
 */
static HResult HPutTermChar(int64_t termno, char c)
{
	/* H_PUT_TERM_CHAR(int64_t termno, int64_t len,
	 * 		   uint64_t char0_7, uint64_t char8_15)
	 * → return_code
	 */
	register uint64_t r3 __asm__("r3") = H_PUT_TERM_CHAR; /* in: hypercall opcode, out: return code */
	register uint64_t r4 __asm__("r4") = termno;
	register uint64_t r5 __asm__("r5") = 1; /* len */
	register uint64_t r6 __asm__("r6") = ((uint64_t)c) << 56;

	__asm__ __volatile__ (
		"sc 1"
	      :"=r"(r3)
	      :"r"(r3), "r"(r4), "r"(r5), "r"(r6)
	);

	return (HResult)r3;	
}

/* HGetTermChar {{{3
 * ------------
 * Hypercall to get characters from console.
 */
static HResult HGetTermChar(int64_t termno, char *buf, uint64_t *buf_len)
{
	register uint64_t r3 __asm__("r3") = H_GET_TERM_CHAR;
	register uint64_t r4 __asm__("r4") = termno; /* in: termno, out: num bytes */
	register uint64_t r5 __asm__("r5");
	register uint64_t r6 __asm__("r6");

	__asm__ __volatile__ (
		"sc 1"
		:"=r"(r3), "=r"(r4), "=r"(r5), "=r"(r6)
		:"r"(r3), "r"(r4)
	);

	if (r3 == H_SUCCESS) {
		for (uint64_t i = 0; i < r4; ++i)
			buf[i] = ((i >= 8 ? r6 : r5) >> (56 - ((i % 8) * 8))) & 0xFF;

		*buf_len = r4;
	}

	return (HResult)r3;
}

/* Architecture {{{2
 * ============
 */

/* Definitions {{{3
 * ===========
 */
#define BIT(N)		(1UL<<(N))	/* Conventional bit numbering */
#define BBT(N)		BIT(63-(N))	/* IBM bit numbering */

#define REG_MSR__SF	BBT(0)	/* 64-Bit Mode */
#define REG_MSR__TA	BBT(1)	/* Tags Active */

#define REG_XER__T02	BBT(41)
#define REG_XER__T07	BBT(42)
#define REG_XER__TAG	BBT(43)

/* SETTAG instruction. Sets XER.TAG=1. Takes no operands. */
#define SETTAG ".long 0x7C0103E6\n"

/* Utility Functions {{{3
 * =================
 */

/* MsrGet {{{4
 * ------
 * Get value of Power ISA MSR register.
 */
static inline uint64_t MsrGet(void)
{
	register uint64_t r3 __asm__("r3");

	__asm__ __volatile__ (
		"mfmsr	%0"
		:"=r"(r3)
	);

	return r3;
}

/* MsrSet {{{4
 * ------
 * Set value of Power ISA MSR register.
 */
static inline void MsrSet(uint64_t msr)
{
	__asm__ __volatile__ (
		"mtmsrd %0"
		::"r"(msr)
	);
}

/* MsrMaskOr {{{4
 * ---------
 * Masks and ORs bits in the Power ISA MSR register.
 */
static inline void MsrMaskOr(uint64_t mask, uint64_t or_)
{
	MsrSet((MsrGet() & ~mask) | or_);
}

/* XerGet {{{4
 * ------
 * Get value of Power ISA XER register.
 */
static inline uint64_t XerGet(void)
{
	register uint64_t r3 __asm__("r3");

	__asm__ __volatile__ (
		"mfxer	%0"
		:"=r"(r3)
	);

	return r3;
}

/* Fat Pointer Management {{{2
 * ======================
 */

/* Definitions {{{3
 * -----------
 */

/* Structure to represent a 128-bit pointer. By convention,
 * the 64-bit memory address (the actual pointer) goes in w[1];
 * w[0] is reserved for metadata. Must be 16-byte aligned. */
typedef struct __attribute__((__aligned__(16))) {
	uint64_t w[2];
} Ptr;

/* Ptr structure which never has tag bits set on it. Used to quickly
 * clear XER.TAG. */
Ptr g_ptrDummy;

/* PPC AS-Specific Architecture Utilities {{{3
 * ======================================
 */

/* XerTagSet {{{4
 * ---------
 * Sets XER.TAG=1 (XER bbt 43).
 */
static inline void XerTagSet(void)
{
	__asm__ __volatile__ (
		SETTAG
	);
}

/* XerTagClear {{{4
 * -----------
 * Sets XER.TAG=0 (XER bbt 43).
 */
static inline void XerTagClear(void)
{
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");

	g_ptrDummy.w[0] = 0;

	__asm__ __volatile__ (
		"lq	%0, 0(%1)"
		:"=r"(w0), "=r"(w1)
		:"r"(&g_ptrDummy)
	);
}

/* TagsOn {{{4
 * ------
 * Sets MSR.TA=1 (MSR bbt 1).
 */
static inline void TagsOn(void)
{
	MsrMaskOr(0, REG_MSR__TA);
}

/* TagsOff {{{4
 * -------
 * Sets MSR.TA=0 (MSR bbt 1).
 */
static inline void TagsOff(void)
{
	MsrMaskOr(REG_MSR__TA, 0);
}


/* PtrBless {{{3
 * --------
 * Sets tag bit on the quadword at *p.
 *
 * Note: This would not be safe to use if the tag bit is being used to enforce
 * a security invariant and concurrent access to *p by untrusted code is
 * possible, due to race conditions. See PtrStoreBless for a more secure
 * construction.
 */
static inline void PtrBless(Ptr *p)
{
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");

	__asm__ __volatile__ (
	       NL "lq	%0, 0(%1)"
	       NL SETTAG
	       NL "stq  %0, 0(%1)"
	      :"=&r"(w0), "=&r"(w1)
	      :"r"(p)
	);
}

/* PtrUnbless {{{3
 * ----------
 * Clears tag bit on the quadword at *p.
 */
static inline void PtrUnbless(Ptr *p)
{
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");
	register uint64_t w2 __asm__("r6");
	register uint64_t w3 __asm__("r7");

	// Could also be implemented as:
	//   *(volatile uint8_t *)p |= 0;
	// as any write clears the tag bits.

	__asm__ __volatile__ (
	       NL "lq	%0, 0(%4)"
	       NL "lq   %2, 0(%5)"
	       NL "stq  %0, 0(%4)"
	      :"=&r"(w0), "=&r"(w1), "=&r"(w2), "=&r"(w3)
	      :"r"(p), "r"(&g_ptrDummy)
	);
}

/* PtrValid {{{3
 * --------
 * Returns true if tag bit is set on *p.
 */
static inline bool PtrValid(const Ptr *p)
{
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");
	uint64_t xer;

	__asm__ __volatile__ (
		NL "lq     %0, 0(%3)"
		NL "mfxer  %2"
		NL "lq	   %0, 0(%4)"
	       :"=&r"(w0), "=&r"(w1), "=&r"(xer)
	       :"r"(p), "r"(&g_ptrDummy)
	);

	return !!(xer & REG_XER__TAG);
}

/* PtrSel {{{3
 * ------
 * Demonstrates use of SELII instruction. Returns ifValid if tag bit is set on
 * *p, ifInvalid otherwise.
 */
static inline uint64_t PtrSel(const Ptr *p, uint64_t ifValid, uint64_t ifInvalid)
{
	register uint64_t r __asm__("r3");
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");
	register uint64_t ifValid_ __asm__("r6") = ifValid;
	register uint64_t ifInvalid_ __asm__("r7") = ifInvalid;

	__asm__ __volatile__ (
		NL "lq     %0, 0(%2)"
		// selrr   %r3, %r6, %r7, TAG  // (TAG = 43-32 = 11)
		NL ".long  0x78c33d9e"
		NL "lq     %0, 0(%3)"
	       :"=&r"(w0), "=&r"(w1)
	       :"r"(p), "r"(&g_ptrDummy), "r"(ifValid_), "r"(ifInvalid_)
	);

	return r;
}

/* PtrValidAlt {{{3
 * -----------
 * Demonstration alternate implementation of PtrValid(). Identical semantics.
 */
static inline bool PtrValidAlt(const Ptr *p)
{
	return PtrSel(p, 1, 0);
}

/* PtrStoreBless {{{3
 * -------------
 * Store *src to *dst, blessing it in the process. Safer than PtrBless.
 *
 * Note: If using tag bits to enforce a security invariant, *src must be a
 * trusted location in memory which cannot be concurrently accessed by
 * untrusted code.
 */
static inline void PtrStoreBless(Ptr *dst, const Ptr *src)
{
	__asm__ __volatile__ (
		NL "lq	     %%r4, 0(%1)"
		NL SETTAG
		NL "stq      %%r4, 0(%0)"
		// Clear tag in XER
		NL "lq       %%r4, 0(%2)"
	     :: "r"(dst), "r"(src), "r"(&g_ptrDummy)
	      : "r4", "r5"
	);
}

/* PtrCopy {{{3
 * -------
 * Copies *src to *dst. Any tag bits are preserved, if set.
 */
static inline void PtrCopy(Ptr *dst, const Ptr *src)
{
	__asm__ __volatile__ (
		NL "lq	     %%r4, 0(%1)"
		NL "stq      %%r4, 0(%0)"
		// Clear tag in XER
		NL "lq       %%r4, 0(%2)"
	     :: "r"(dst), "r"(src), "r"(&g_ptrDummy)
	      : "r4", "r5"
	);
}

/* PtrLoad {{{3
 * -------
 * Loads a pointer from *p to *dst. Returns true if the tag bit was set.
 */
static inline bool PtrLoad(const Ptr *p, Ptr *dst)
{
	register uint64_t w0 __asm__("r4");
	register uint64_t w1 __asm__("r5");
	uint64_t xer;

	__asm__ __volatile__ (
		NL "lq     %0, 0(%3)"
		NL "mfxer  %2"
	       :"=&r"(w0), "=&r"(w1), "=&r"(xer)
	       :"r"(p)
	);

	if (!(xer & REG_MSR__TA)) {
		dst->w[0] = 0;
		dst->w[1] = 0;
		return false;
	}

	dst->w[0] = w0;
	dst->w[1] = w1;
	return true;
}

/* PtrLoadW1 {{{3
 * ---------
 * Loads the second half of *p, returning it if the tag bit is set and
 * returning NULL otherwise. Uses the LTPTR instruction which was probably
 * added in POWER7.  Won't work on earlier machines. (See US patent
 * US20090198967A1.)
 */
static inline void *PtrLoadW1(const Ptr *p)
{
	register uint64_t w1 __asm__("r3") = (uint64_t)p;

	// ltptr RT,DQ(RA),EPT
	//
	// 57  | RT   | RA    | DQ    | EPT   | 1     |
	// 0:7 | 8:10 | 11:17 | 18:27 | 28:29 | 30:31 |  fake (as shown in patent)
	// 0:5 | 6:10 | 11:15 | 16:27 | 28:29 | 30:31 |  real (guessed)
	__asm__ __volatile__ (
		// ltptr  %r3, 0(%r3), 3
		NL ".long 0xe463000d"
	      :"=r"(w1)
	      :"r"(w1)
	);

	return (void *)w1;
}

/* I/O Utilities {{{2
 * =============
 */

/* PutChar {{{3
 * -------
 * Writes a character to the console.
 */
static void PutChar(char c)
{
	while (HPutTermChar(0, c) == H_BUSY);
}

/* Print {{{3
 * -----
 */
static void Print(const char *s)
{
	while (*s)
		PutChar(*s++);
}

/* Printf {{{3
 * ------
 */
static char _HexChar(uint8_t x)
{
	if (x >= 10)
		return 'a' + x - 10;
	return '0' + x;
}

static void PrintHex(uint64_t x)
{
	for (int i = 60; i >= 0; i -= 4)
		PutChar(_HexChar((x >> i) & 0xf));
}

static void PrintUInt(uint64_t x)
{
	char buf[32], *p = buf + 31;

	if (!x) {
		PutChar('0');
		return;
	}

	*p-- = '\0';
	for (; x; x /= 10)
		*p-- = '0' + x % 10;

	Print(p + 1);
}

static void VPrintf(const char *fmt, va_list args)
{
	char c;
	bool esc = false;

	for (;; ++fmt) {
		if (esc) {
			esc = false;
			switch (c = *fmt) {
				case '%':
					PutChar('%');
					break;
				case 's':
					Print(va_arg(args, const char *));
					break;
				case 'u':
					PrintUInt(va_arg(args, uint64_t));
					break;
				case 'x':
					PrintHex(va_arg(args, uint64_t));
					break;
				case 'c':
					PutChar(va_arg(args, int));
					break;
				case '\0':
					return;
				default:
					PutChar(c);
					break;
			}
		} else switch (c = *fmt) {
			case '%':
				esc = true;
				break;
			case '\0':
				return;
			default:
				PutChar(c);
				break;
		}
	}
}

static void Printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	VPrintf(fmt, args);
	va_end(args);
}

/* Halt {{{3
 * ----
 * Halt and do not return.
 */
noreturn void Halt(void)
{
	for (;;);
}

/* Initialization {{{2
 * ==============
 */

/* BssInit {{{3
 * -------
 * Zeroes all of the BSS space.
 */
extern char _bss_start[];
extern char _bss_end[];

static void BssInit(void)
{
	uint64_t *p   = (uint64_t *)_bss_start,
		 *end = (uint64_t *)_bss_end;

	while (p < end)
		*p++ = 0;
}

/* Test Program {{{2
 * ============
 */
Ptr test_ptr;

int main(void)
{
	BssInit();

	Print("\n\n\n============================================\n");
	Print("TA Test Utility\n");
	Printf("MSR: 0x%x SF=%u TA=%u\n", MsrGet(), !!(MsrGet() & REG_MSR__SF), !!(MsrGet() & REG_MSR__TA));
	TagsOn();
	Printf("MSR: 0x%x SF=%u TA=%u\n", MsrGet(), !!(MsrGet() & REG_MSR__SF), !!(MsrGet() & REG_MSR__TA));
	if (!(MsrGet() & REG_MSR__TA)) {
		Printf("ERROR: Cannot set Tags Active mode, environment not supported.\n");
		Halt();
	} else
		Printf("SUCCESS: Successfully activated Tags Active mode.\n");

	// Create an unblessed pointer.
	Ptr p;
	p.w[0] = 42;
	p.w[1] = 69;
	PtrCopy(&test_ptr, &p);
	Printf("Test Pointer (pre-bless):  Valid=%u Ptr=%x\n", PtrValid(&test_ptr), PtrLoadW1(&test_ptr));
	if (PtrValid(&test_ptr) || PtrValidAlt(&test_ptr))
		Printf("ERROR: Pointer should not be valid\n");

	// Bless the pointer.
	PtrStoreBless(&test_ptr, &p);
	Printf("Test Pointer (post-bless): Valid=%u Ptr=%x\n", PtrValid(&test_ptr), PtrLoadW1(&test_ptr));
	if (!PtrValid(&test_ptr) || !PtrValidAlt(&test_ptr))
		Printf("ERROR: Pointer should be valid\n");

	// Invalidate the pointer by modifying it.
	test_ptr.w[1] = 69;
	Printf("Test Pointer (post-write): Valid=%u Ptr=%x\n", PtrValid(&test_ptr), PtrLoadW1(&test_ptr));
	if (PtrValid(&test_ptr) || PtrValidAlt(&test_ptr))
		Printf("ERROR: Pointer should be valid\n");

	Printf("Test program complete.\n\n\n");
	Halt();
	return 0;
}
