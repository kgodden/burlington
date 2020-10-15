

 
// Attempted c64 Emulator
// http://nesdev.com/6502_cpu.txt
// https://sites.google.com/site/6502asembly/
// http://www.obelisk.me.uk/6502/reference.html
// https://wiki.nesdev.com/w/index.php/Status_flags


// IRQ:
// https://www.pagetable.com/?p=410
//
// Online assembler
// http://www.cs.otago.ac.nz/cosc243/resources/6502js-master/globalvars.html

// ToDo:
// Check addressing modes
// don't have all indirect indexed modes
#include "burlington.h"
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <stdio.h>

//#define C_DEBUG_PRINTF 1

#if C_DEBUG_PRINTF
    #define pf(fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while (0);
#else
    #define pf(...)
#endif

// graphics.cpp
char get_key();

// Registers
typedef unsigned char reg_t;
typedef unsigned char byte_t;
typedef unsigned int moff_t;
typedef unsigned char line_t;

moff_t pc;   // Program,Counter
reg_t sp;    // Stack,Pointer
reg_t s;    // Processor,Status
reg_t* aa;    // A
reg_t x;    // X Register
reg_t y;    // Y Register

#define MEM_SIZE ((moff_t)(1024 * 64))

#define A *aa

#define PCL (pc & 0xFF)
#define PCH ((pc >> 8) & 0xFF)

// A is stored one byte after the top of memory
#define A_ADDR MEM_SIZE

// Status bits
#define C (1 << 0)
#define Z (1 << 1)
#define I (1 << 2)
#define D (1 << 3)
#define B (1 << 4)
#define S_UNUSED (1 << 5)
#define V (1 << 6)
#define N (1 << 7)

#define BIT_OVERFLOW (1 << 8)
#define BIT_7  (1 << 7)
#define BIT_6  (1 << 7)
#define BIT_0  1

#define STACK_BASE 0x0100

const char* get_address_mode_name(unsigned int code);
const char* get_op_name(unsigned int code);

// Operand storage
reg_t o;
moff_t addr;

reg_t* memory;

static bool vram_dirty = false;

line_t IRQ = 0;

// opcodes
typedef void (*op_t)();

// Address modes
typedef reg_t (*addr_mode_t)();

reg_t mem(moff_t addr) {
    // Hack, $fe -> random number gen
    if (addr == 0xfe) {
        reg_t r = (rand() * 0xFF) / RAND_MAX;
        pf("RAND: %02x\n", r);
        return r;
    } else if (addr == 0xFF) {
        char key = get_key();
        //printf("Key: %d\n", key);
        return key;
    }
    
    return *(memory + addr);
}

void smem(moff_t addr, reg_t val) {
    *(memory + addr) = val;

    if (addr == 0xfd) {
        printf("%c", val);
        fflush(stdout);
    }
    
    if (addr >= 0x200 && addr <= 0x5ff) {
        pf("SMEM: %04x, %02x\n", addr, val);
        vram_dirty = true;
        // Hack char output reg..
//        exit(0);
    }
}

void push(reg_t val) {
    if (sp == 0) {
        printf("Error, Stack underflow!\n");
        exit(-1);
    }
    
    smem(STACK_BASE + sp--, val);
}

reg_t pop() {
    return mem(STACK_BASE + ++sp);
}

void update_ZN(reg_t v) {

    // Zero flag?
    if (v)
        s &= ~Z;
     else
        s |= Z;
        
     // Negative?
     // Signed numbers stored as 2's compliment
     // MSB indicates -ve.
     if (v & BIT_7)
        s |= N;
      else
        s &= ~N;
        
}

void update_carry_7(reg_t v) {
    if (v & BIT_7)
        s |= C;
    else
        s &= ~C;
}

void update_carry_0(reg_t v) {
    if (v & 1)
        s |= C;
    else
        s &= ~C;
}

reg_t copy_bit(reg_t src, reg_t dest, reg_t mask) {
    return (dest & ~mask) | (src & mask);
}

void raise_IRQ(int set_B) {

    // https://www.pagetable.com/?p=410

    if (!(s & I)) {    // Is this correct?
        push(PCH);
        push(PCL);
            
        push(set_B ? s | B : s);
        IRQ = 1;
        
        reg_t pcl = mem(0xFFFE);
        reg_t pch = mem(0xFFFF);
        pc = (pch << 8) | pcl;
        pf("Raising IRQ, I=%d, B=%d --> 0x%04x\n", s & I, set_B, pc);
    } else {
        pf("NOT Raising IRQ, I=%d, B=%d", s & I, set_B);
    }
}


//
// The OPCODES
//
void brk() {
    raise_IRQ(1);
}


void jsr() {
    // this is may be wrong should push pc - 1!!
    // high byte first    
    // http://6502.org/tutorials/interrupts.html
    push(pc >> 8);
    push(pc & 0xFF);
    pc = addr;    
}

void rti() {
    // Must clear I, The interrupt handler sets I to prevent more
    // IRQ's while servicing this one. I is automatically cleared when we pop
    // S off the stack (as it would have been 0 to allow an interrupt
    // occur in the first place!)
    //
    reg_t pch = pop();
    reg_t pcl = pop();
    s = pop() & ~B;     // Clear B pseudo-flag
    
    pc = (pch << 8) | pcl;
    
}

void rts() {
    // this may be wrong, might have to set pc + 1?
    // http://6502.org/tutorials/interrupts.html
    pc = (pop() | (pop() << 8));
}

void nop() {
}

void ldy() {
    y = o;
    update_ZN(y);
}

void cpy() {
    if (y >= o)
        s |= C;
    else
        s &= ~C;
        
    if (y == o)
        s |= Z;
    else
        s &= ~Z;

    // Not sure about this!
    if ((y - o) & (1 << 7))    
        s |= N;
    else
        s &= ~N;
}

void cpx() {
    if (x >= o)
        s |= C;
    else
        s &= ~C;
        
    if (x == o)
        s |= Z;
    else
        s &= ~Z;

    // Not sure about this!
    if ((x - o) & (1 << 7))    
        s |= N;
    else
        s &= ~N;
}

void ora() {
    A |= o;
    update_ZN(A);
}

void nnd() {
    A &= o;
    update_ZN(A);
}

void eor() {
    A ^= o;
    update_ZN(A);
}

void adc() {
    //A += o;
    //reg_t oldA = A;
    // Using shorts to detect 8bit overflow/carry
    unsigned short a1 = A;
    unsigned short a2 = o;
    unsigned short r = a1 + a2;

    if (s & C)
        ++r;
    
    // grab lower 8 bits
    A = r & 0xFF;
 
/* 
    if (r > 0xff)
        pf("-------------------ADC carry!\n");
*/
    
    // Did we overflow?
    if (r & 0x100)
        s |= C;
     else
        s &= ~C;
    
    // Need to set V!
    
    //pf("ADC: A=%02x,o=%02x,r=%02x\n", oldA, o, A);
    update_ZN(A);
}

void sta() {
    // Doesn't touch any flags!
    pf("STA #$%02x --> $%02x\n", A, addr);
    smem(addr, A);
}

void lda() {
    A = o;
    update_ZN(A);
}

void cmp() {
    
    if (A >= o)
        s |= C;
    else
        s &= ~C;
        
    if (A == o)
        s |= Z;
    else
        s &= ~Z;
        
    // Not sure about this!
    if ((A - o) & (1 << 7))    
        s |= N;
    else
        s &= ~N;
}

// Incomplete... C in?
void sbc() {
    // Take 2's complement of operand to make it 
    // -ve. then add it to A
    // What about the input C???????
    o = ~o - 1;
    adc();
}

void ttt() {
    pf("++++++ TTT not implemented\n");
}

void slo() {
    pf("++++++ SLO not implemented\n");
}

void rla() {
    pf("++++++ RLA not implemented\n");
}

void sre() {
    pf("++++++ SREx not implemented\n");
}

void rra() {
    pf("++++++ RRA not implemented\n");
}


void sax() {
    pf("++++++ SAX not implemented\n");
}

void lax() {
    pf("++++++ LAX not implemented\n");
}

void dcp() {
    pf("++++++ DCP not implemented\n");
}

void isb() {
    pf("++++++ ISB not implemented\n");
}

void bit() {
    reg_t r = mem(addr);
    
    if (r)
        s &= ~Z;
    else
        s |= Z;

    if (r & BIT_6)
        s |= V;
    else
        s &= ~V;

    if (r & BIT_7)
        s |= N;
    else
        s &= ~N;
        
}

void sha() {
    pf("++++++ SHA not implemented\n");
}


void inc() {
    update_ZN(++o);
    smem(addr, o);
}

void dec() {
    update_ZN(--o);
    smem(addr, o);
}

void ldx() {
    x = o;
    update_ZN(x);
}

void shx() {
    pf("++++++ SHX not implemented\n");
}

void ror() {
    // reviewed 25/4/18

    // Use short to bring in carry
    unsigned short val = mem(addr);

    if (s & C)
        val |= BIT_OVERFLOW;
    
    if (val & BIT_0)
        s |= C;
    else
        s &= ~C;
    
    val >>= 1;
        
    update_ZN(val);
    smem(addr, val);
}

void lsr() {
    // reviewed 25/4/18
    reg_t val = mem(addr);
       
    if (val & BIT_0)
        s |= C;
    else
        s &= ~C;

        val >>= 1;

    update_ZN(val);
    smem(addr, val);
}

void rol() {
    // reviewed 25/4/18

    // use short to capture overflow bit
    unsigned short val = mem(addr);
    
    val <<= 1;

    // Bring carry into LSB
    val |= (s & C) ? 1 : 0;

    // Set carry if we overflowed
    if (val & BIT_OVERFLOW)
        s |= C;
    else
        s &= ~C;
        
    update_ZN(val);
    
    smem(addr, val);
}

void asl() {
    // reviewed 25/4/18
    reg_t val = mem(addr);
    
    update_carry_7(val);
    
    val <<= 1;
    
    update_ZN(val);

    smem(addr, val);
}

void shy() {
    pf("not implemented - shy()");
}

void las() {
    pf("not implemented - las()");
}

void shs() {
    pf("not implemented - shs()");
}

void tsx() {
    x = sp;
    update_ZN(x);
}

void txs() {
    sp = x;
}

void sed() {
    s |= V;
}

void cld() {
    s &= ~D;
}

void clv() {
    s &= ~V;
}

void tya() {
    A = y;
    update_ZN(A);
}

void sei() {
    s |= I;
}

void cli() {
    s &= ~I;
}

void sec() {
    s |= C;
}

void clc() {
    s &= ~C;
}

void stx() {
    smem(addr, x);
}

void sty() {
    smem(addr, y);
}

void branch_rel(reg_t offset) {
    reg_t pcl = (pc & 0xFF) + o;
    pc = (pc & 0xFF00) | pcl;
    pf("branch 0x%02x -> 0x%04x\n", o, pc);
}

void beq() {
    if (s & Z) {
        branch_rel(o);
    }
    pf("branch 0x%02x -> 0x%04x\n", o, pc);
}

void bne() {
    pf("s=%02x\n", s & Z);
    if ((s & Z) == 0) {
        branch_rel(o);
    }
}

void bcs() {
    if (s & C) {
        branch_rel(o);
    }
}

void bcc() {
    if ((s & C) == 0) {
        branch_rel(o);
    }
}

void bvs() {
    if ((s & V) != 0) {
        branch_rel(o);
    }
}

void bvc() {
    if ((s & V) == 0) {
        branch_rel(o);
    }
}

void bmi() {
    if (s & N) {
        branch_rel(o);
    }
}

void bpl() {
    if ((s & N) == 0) {
        branch_rel(o);
    }
}

void jmp() {
    pc = addr;
}

void sbx() {
    pf("not implemented - sbx()");
}


void lxa() {
    pf("not implemented - lxa``()");
}

void ane() {
    pf("not implemented - ane()");
}

void arr() {
    pf("not implemented - arr()");
}

void asr() {
    pf("not implemented - asr()");
}

void anc() {
    pf("++++++ ANC not implemented\n");
}

void dex() {
    update_ZN(--x);
}

void tax() {
    x = A;
    update_ZN(x);
}

void txa() {
    A = x;
    update_ZN(A);
}

void inx() {
    update_ZN(++x);
}

void iny() {
    update_ZN(++y);
}

void tay() {
    y = A;
    update_ZN(y);
}

void dey() {
    update_ZN(--y);
}

void pla() {
    A = pop();
    update_ZN(A);
}

void pha() {
    push(A);
}

void plp() {
    // https://wiki.nesdev.com/w/index.php/Status_flags
    s = pop() &~ B; // Ignore B
}

void php() {
    push(s);
}


op_t opcodes[] = {
brk, jsr, rti, rts, nop, ldy, cpy, cpx, // +00 impl/immed
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +01 indir,x
ttt, ttt, ttt, ttt, nop, ldx, nop, nop, // +02 ? /immed
slo, rla, sre, rra, sax, lax, dcp, isb, // +03 indir,x
nop, bit, nop, nop, sty, ldy, cpy, cpx, // +04 zeropage
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +05 zeropage
asl, rol, lsr, ror, stx, ldx, dec, inc, // +06 zeropage
slo, rla, sre, rra, sax, lax, dcp, isb, // +07 zeropage
php, plp, pha, pla, dey, tay, iny, inx, // +08 implied
ora, nnd, eor, adc, nop, lda, cmp, sbc, // +09 immediate
asl, rol, lsr, ror, txa, tax, dex, nop, // +0a accu/impl
anc, anc, asr, arr, ane, lxa, sbx, sbc, // +0b immediate
nop, bit, jmp, jmp, sty, ldy, cpy, cpx, // +0c absolute
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +0d absolute
asl, rol, lsr, ror, stx, ldx, dec, inc, // +0e absolute
slo, rla, sre, rra, sax, lax, dcp, isb, // +0f absolute
bpl, bmi, bvc, bvs, bcc, bcs, bne, beq, // +10 relative
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +11 indir,y
ttt, ttt, ttt, ttt, ttt, ttt, ttt, ttt, // +12 ?
slo, rla, sre, rra, sha, lax, dcp, isb, // +13 indir,y
nop, nop, nop, nop, sty, ldy, nop, nop, // +14 zeropage,x
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +15 zeropage,x
asl, rol, lsr, ror, stx, ldx, dec, inc, // +16 zeropage,x
slo, rla, sre, rra, sax, lax, dcp, isb, // +17 zeropage,x
clc, sec, cli, sei, tya, clv, cld, sed, // +18 implied
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +19 absolute,y
nop, nop, nop, nop, txs, tsx, nop, nop, // +1a implied
slo, rla, sre, rra, shs, las, dcp, isb, // +1b absolute,y
nop, nop, nop, nop, shy, ldy, nop, nop, // +1c absolute,x
ora, nnd, eor, adc, sta, lda, cmp, sbc, // +1d absolute,x
asl, rol, lsr, ror, shx, ldx, dec, inc, // +1e absolute,x
slo, rla, sre, rra, sha, lax, dcp, isb, // +1f absolute,x
};

const char* op_names[] = {
"brk", "jsr", "rti", "rts", "nop", "ldy", "cpy", "cpx", // +00 impl/immed
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +01 indir,x
"ttt", "ttt", "ttt", "ttt", "nop", "ldx", "nop", "nop", // +02 ? /immed
"slo", "rla", "sre", "rra", "sax", "lax", "dcp", "isb", // +03 indir,x
"nop", "bit", "nop", "nop", "sty", "ldy", "cpy", "cpx", // +04 zeropage
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +05 zeropage
"asl", "rol", "lsr", "ror", "stx", "ldx", "dec", "inc", // +06 zeropage
"slo", "rla", "sre", "rra", "sax", "lax", "dcp", "isb", // +07 zeropage
"php", "plp", "pha", "pla", "dey", "tay", "iny", "inx", // +08 implied
"ora", "and", "eor", "adc", "nop", "lda", "cmp", "sbc", // +09 immediate
"asl", "rol", "lsr", "ror", "txa", "tax", "dex", "nop", // +0a accu/impl
"anc", "anc", "asr", "arr", "ane", "lxa", "sbx", "sbc", // +0b immediate
"nop", "bit", "jmp", "jmp", "sty", "ldy", "cpy", "cpx", // +0c absolute
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +0d absolute
"asl", "rol", "lsr", "ror", "stx", "ldx", "dec", "inc", // +0e absolute
"slo", "rla", "sre", "rra", "sax", "lax", "dcp", "isb", // +0f absolute
"bpl", "bmi", "bvc", "bvs", "bcc", "bcs", "bne", "beq", // +10 relative
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +11 indir,y
"ttt", "ttt", "ttt", "ttt", "ttt", "ttt", "ttt", "ttt", // +12 ?
"slo", "rla", "sre", "rra", "sha", "lax", "dcp", "isb", // +13 indir,y
"nop", "nop", "nop", "nop", "sty", "ldy", "nop", "nop", // +14 zeropage,x
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +15 zeropage,x
"asl", "rol", "lsr", "ror", "stx", "ldx", "dec", "inc", // +16 zeropage,x
"slo", "rla", "sre", "rra", "sax", "lax", "dcp", "isb", // +17 zeropage,x
"clc", "sec", "cli", "sei", "tya", "clv", "cld", "sed", // +18 implied
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +19 absolute,y
"nop", "nop", "nop", "nop", "txs", "tsx", "nop", "nop", // +1a implied
"slo", "rla", "sre", "rra", "shs", "las", "dcp", "isb", // +1b absolute,y
"nop", "nop", "nop", "nop", "shy", "ldy", "nop", "nop", // +1c absolute,x
"ora", "and", "eor", "adc", "sta", "lda", "cmp", "sbc", // +1d absolute,x
"asl", "rol", "lsr", "ror", "shx", "ldx", "dec", "inc", // +1e absolute,x
"slo", "rla", "sre", "rra", "sha", "lax", "dcp", "isb", // +1f absolute,x
};


moff_t read_addr() {
    moff_t a = (mem(pc) | mem(pc + 1) << 8);
    pc += 2;
    return a;
}


reg_t accu() {
    // Read and throw away
    //mem(pc++);
    o = A;
    addr = A_ADDR;
    return o;
}

reg_t unk() {
    // Read and throw away
    mem(pc++);
    return o;
}

reg_t impl() {
    pf("\n");
    return o;
}

reg_t immed() {
    o = mem(pc++);
    pf(" #$%02x\n", o);
    return o;
}

reg_t zero() {
    addr = mem(pc++);
    pf(" $%02x\n", addr);
    return mem(addr);
}

reg_t zero_x() {
    addr = mem(pc++) + x;
    return mem(addr);
}

reg_t zero_y() {
    addr = mem(pc++) + y;
    return mem(addr);
}

reg_t rel() {
    o = mem(pc++);
    return o;
}

reg_t abs() {
    addr = read_addr();

    pf(" $%04x\n", addr);
    return mem(addr);
}

reg_t abs_x() {
    addr = read_addr();

    pf(" $%04x,x --> $%04x\n", addr, addr + x);
    addr += x;
    return mem(addr);
}

reg_t abs_y() {
    addr = read_addr() + y;

    return mem(addr);
}

reg_t indir() {
    // jmp only?
    addr = read_addr();

    moff_t new_addr = (mem(addr) | mem(addr + 1) << 8); 
    pf(" ($%04x) -> $%04x\n", addr, new_addr);
    addr = new_addr;
    return o;
}

// working on this
reg_t indir_x() {
    moff_t zp = mem(pc++ + x);
    addr = (mem(zp) | mem(zp + 1) << 8);
    pf(" ($%02x),x -> $%04x\n", zp, addr);
    return mem(addr);
}

reg_t indir_y() {
    moff_t zp = mem(pc++);
    addr = (mem(zp) | mem(zp + 1) << 8) + y;
    pf(" ($%02x),y -> $%04x\n", zp, addr);

    return mem(addr);
}

addr_mode_t address_modes[] = {
impl, abs, impl, impl, impl, immed, immed, immed, // +00 impl/immed
indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, // +01 indir,x
immed, immed, immed, immed, immed, immed, immed, immed, // +02 ? /immed
indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, indir_x, // +03 indir,x
zero, zero, zero, zero, zero, zero, zero, zero, // +04 zeropage
zero, zero, zero, zero, zero, zero, zero, zero, // +05 zeropage
zero, zero, zero, zero, zero, zero, zero, zero, // +06 zeropage
zero, zero, zero, zero, zero, zero, zero, zero, // +07 zeropage

impl, impl, impl, impl, impl, impl, impl, impl, // +08 implied
immed, immed, immed, immed, immed, immed, immed, immed, // +09 immediate
accu, accu, accu, accu, impl, impl, impl, impl, // +0a accu/impl
immed, immed, immed, immed, immed, immed, immed, immed, // +0b immediate
abs, abs, abs, indir, abs, abs, abs, abs, // +0c absolute
abs, abs, abs, abs, abs, abs, abs, abs, // +0d absolute
abs, abs, abs, abs, abs, abs, abs, abs, // +0e absolute
abs, abs, abs, abs, abs, abs, abs, abs, // +0f absolute

rel, rel, rel, rel, rel, rel, rel, rel, // +10 relative
indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, // +11 indir,y
unk, unk, unk, unk, unk, unk, unk, unk, // +12 ?
indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, indir_y, // +13 indir,y
zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, // +14 zeropage,x
zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, zero_x, // +15 zeropage,x
zero_x, zero_x, zero_x, zero_x, zero_y, zero_y, zero_x, zero_x, // +16 zeropage,x
zero_x, zero_x, zero_x, zero_x, zero_y, zero_y, zero_x, zero_x, // +17 zeropage,x

impl, impl, impl, impl, impl, impl, impl, impl, // +18 implied
abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, // +19 absolute,y
impl, impl, impl, impl, impl, impl, impl, impl, // +1a implied
abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, abs_y, // +1b absolute,y
abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, // +1c absolute,x
abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, abs_x, // +1d absolute,x
abs_x, abs_x, abs_x, abs_x, abs_y, abs_y, abs_x, abs_x, // +1e absolute,x
abs_x, abs_x, abs_x, abs_x, abs_y, abs_y, abs_x, abs_x, // +1f absolute,x
};

static const char* address_mode_names[] = {
 "impl", "abs", "impl", "impl", "impl", "immed", "immed", "immed",                          // +00 impl/immed
 "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x",    // +01 indir,x
 "immed", "immed", "immed", "immed", "immed", "immed", "immed", "immed",                    // +02 ? /immed
 "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x", "indir_x",    // +03 indir,x
 "zero", "zero", "zero", "zero", "zero", "zero", "zero", "zero",                            // +04 zeropage
 "zero", "zero", "zero", "zero", "zero", "zero", "zero", "zero",                            // +05 zeropage
 "zero", "zero", "zero", "zero", "zero", "zero", "zero", "zero",                            // +06 zeropage
 "zero", "zero", "zero", "zero", "zero", "zero", "zero", "zero",                            // +07 zeropage

 "impl", "impl", "impl", "impl", "impl", "impl", "impl", "impl",                            // +08 implied
 "immed", "immed", "immed", "immed", "immed", "immed", "immed", "immed",                    // +09 immediate
 "accu", "accu", "accu", "accu", "impl", "impl", "impl", "impl",                            // +0a accu/impl
 "immed", "immed", "immed", "immed", "immed", "immed", "immed", "immed",                    // +0b immediate
 "abs", "abs", "abs", "indir", "abs", "abs", "abs", "abs",                                  // +0c absolute
 "abs", "abs", "abs", "abs", "abs", "abs", "abs", "abs",                                    // +0d absolute
 "abs", "abs", "abs", "abs", "abs", "abs", "abs", "abs",                                    // +0e absolute
 "abs", "abs", "abs", "abs", "abs", "abs", "abs", "abs",                                    // +0f absolute

 "rel", "rel", "rel", "rel", "rel", "rel", "rel", "rel",                                    // +10 relative
 "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y",    // +11 indir,y
 "unk", "unk", "unk", "unk", "unk", "unk", "unk", "unk",// +12 ?
 "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y", "indir_y",    // +13 indir,y
 "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x",            // +14 zeropage,x
 "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x", "zero_x",            // +15 zeropage,x
 "zero_x", "zero_x", "zero_x", "zero_x", "zero_y", "zero_y", "zero_x", "zero_x",            // +16 zeropage,x
 "zero_x", "zero_x", "zero_x", "zero_x", "zero_y", "zero_y", "zero_x", "zero_x",            // +17 zeropage,x

 "impl", "impl", "impl", "impl", "impl", "impl", "impl", "impl",                            // +18 implied
 "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y",                    // +19 absolute,y
 "impl", "impl", "impl", "impl", "impl", "impl", "impl", "impl",                            // +1a implied
 "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y", "abs_y",                    // +1b absolute,y
 "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x",                    // +1c absolute,x
 "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x", "abs_x",                    // +1d absolute,x
 "abs_x", "abs_x", "abs_x", "abs_x", "abs_y", "abs_y", "abs_x", "abs_x",                    // +1e absolute,x
 "abs_x", "abs_x", "abs_x", "abs_x", "abs_y", "abs_y", "abs_x", "abs_x",                    // +1f absolute,x
};


op_t get_op(unsigned int code) {
    unsigned int col = code >> 5;
    unsigned int row = code & 0x1F;

    // here here debug
    //pf("Op %02x (%s), r=%d,c=%d, off=%d, mode=%s\n", code, get_op_name(code),row, col, row * 8 + col, get_address_mode_name(code));
    
    return opcodes[row * 8 + col];
}

const char* get_op_name(unsigned int code) {
    unsigned int col = code >> 5;
    unsigned int row = code & 0x1F;

    //pf("Op r=%d,c=%d, off=%d\n", row, col, row * 8 + col);
    
    return op_names[row * 8 + col];
}

addr_mode_t get_addr_mode(unsigned int code) {
    unsigned int col = code >> 5;
    unsigned int row = code & 0x1F;
    
    //pf("Addr r=%d,c=%d\n", row, col);
    
    return address_modes[row * 8 + col];
}

const char* get_address_mode_name(unsigned int code) {
    unsigned int col = code >> 5;
    unsigned int row = code & 0x1F;
    
    return address_mode_names[row * 8 + col];
}

void init_memory() {
    memory = (reg_t*)malloc(MEM_SIZE + 3);
    memset(memory, 0, MEM_SIZE);
    
    aa = memory + MEM_SIZE;
}

unsigned char* vram_6502() {
    return memory + 0x200;
}


bool is_vram_dirty() {
    return vram_dirty;
}

void clear_vram_dirty() {
    vram_dirty = false;
}

void init_stack() {
    // c64 Stack
    sp = 0xFF;
}

// .prg, first 2 bytes indicates start address, the rest is
// binary.
moff_t load_prg(const char* path) {
    moff_t addr, start_addr;
    int val;
    moff_t addr_high, addr_low;
    int count = 0;
    
    FILE* f = fopen(path, "rb");

    pf("%s -->\n", path);
    
    addr_low = (moff_t)fgetc(f); 
    addr_high = (moff_t)fgetc(f); 
    
    start_addr = (addr_high << 8) | addr_low;
    
    pf("Loading to $%04x", start_addr);
    
    addr = start_addr;
    
    while ((val = fgetc(f)) != EOF) {
        smem(addr++, (byte_t)val);
        ++count;        
    }
 
    pf("%d bytes loaded starting at $%04x\n", count, start_addr);
    
    fclose(f);
    
    return start_addr;
}

moff_t load_program(const char* path, moff_t start) {

    // Does path end in .prg ?
    char *dot = strrchr(path, '.');

    if (dot && !strcmp(dot, ".prg")) {
        // Yes, load .prg
        return load_prg(path); 
    }

    // Samples taken from: http://www.6502asm.com/
    // Assembled her: http://www.e-tradition.net/bytes/6502/assembler.html
    
    int addr;
    int val;
    char c;
    int i;
    
    FILE* f = fopen(path, "rb");

    pf("%s -->\n", path);
    
    // 0600: a9 0f 85 00 85 01 a5 fe 29 03 c9 00 f0 2f c9 01
    // 0610: f0 30 c9 02 f0 22 c6 01 a5 01 29 1f 0a aa bd 47 
    // etc
    
    while (fscanf(f, "%x: ", &addr) == 1) {
                    
        pf("%04x: ", addr);
    
        i = 0;
        
        while (i++ < 16 && fscanf(f, "%x%c", &val, &c) == 2) {
            pf("%02x ", val);
            smem(addr++, val);
            
            if (c != ' ' || c == '\n') {
                pf("EOL\n");
                break;
            }
        }
        
        pf("\n");
    }
    
    fclose(f);
    
    return 0;
}

void jump_vector(moff_t vec, moff_t addr) {
    smem(vec, addr);
    smem(vec + 1, addr >> 8);

}

void reset() {
    reg_t pcl = mem(0xFFFC);
    reg_t pch = mem(0xFFFD);
    
    pc = (pch << 8) | pcl;
    
    pf("RESET: $%04x\n", pc);
}

void init_6502(const char* path) {
    //srand(42);
    pc = 0;          // Program,Counter
    sp = S_UNUSED;    // Stack,Pointer
    s = 0;           // Processor,Status
    //a = 0;           // A
    x = 0;           // X Register
    y = 0;           // Y Register
    o = 0;   
    IRQ = 0;
    
    init_memory();
    init_stack();

    moff_t start = 0x0600;
    
    moff_t start_addr = load_program(path, 0x600);
    
    // If the program included start address
    // then we set this into RESET jump vector
    if (start_addr)
        start = start_addr;
    
    // Set-up jump vector for RESET
    jump_vector(0xFFFC, start);
    
    reset();
}


void dump_mem(moff_t start) {
    moff_t m = start;
    
    for (int j = 0; j != 2; j++) {
        printf("%04x: ", m);

        for (int i = 0; i != 16; i++) {
            printf("%02x ", mem(m++));
        }
        printf("\n");
    }
}

void process_IRQ() {
    // If the IRQ line is high
    if (IRQ & (s & I)) {
    
        // Disable interrupts, will be re-enabled
        // when s is restored from the stack in RTI
        s &= ~I;    
        IRQ = 0;    // is this correct?

        // load interrupt vector and go there
        reg_t pcl = mem(0xFFFE);
        reg_t pch = mem(0xFFFF);
        pc = (pch << 8) | pcl;      
    }
}

int next_6502() {
//pf("--------------------------\n");
    pf("$%04x ", pc); 
    reg_t code = mem(pc++);
            
    //pf("code 0x%02x, %s\n", code, get_op_name(code));
    pf("%s ", get_op_name(code));
    o = (get_addr_mode(code))();
    (get_op(code))();

    process_IRQ();
    // here here debuf
    //pf("A=$%02x,X=$%02x,Y=$%02x,PC=$%04x,$0=$%02x,$1=$%02x,$2=$%02x,$3=$%02x\n", A, x, y, pc, mem(0), mem(1), mem(2), mem(3));

    // For the moment just exit if
    // we hit brk.
    if (code == 0x00) {  // brk
        pf("Have hit BRK\n");
        //return 0;
    }
    
    return 1;
}
/*
int main() {
    init_6502();

    int i = 0;
    while (i++ < 50) {
        next_6502();
        pf("a=$%02x,x=$%02x,y=$%02x\n", A, x, y);
    }
}
*/
