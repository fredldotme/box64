#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <errno.h>

#include "debug.h"
#include "box64context.h"
#include "dynarec.h"
#include "emu/x64emu_private.h"
#include "emu/x64run_private.h"
#include "x64run.h"
#include "x64emu.h"
#include "box64stack.h"
#include "callback.h"
#include "emu/x64run_private.h"
#include "x64trace.h"
#include "dynarec_arm64.h"
#include "dynarec_arm64_private.h"
#include "arm64_printer.h"

#include "dynarec_arm64_helper.h"
#include "dynarec_arm64_functions.h"


uintptr_t dynarec64_F0(dynarec_arm_t* dyn, uintptr_t addr, uintptr_t ip, int ninst, rex_t rex, int rep, int* ok, int* need_epilog)
{
    uint8_t opcode = F8;
    uint8_t nextop, u8;
    uint32_t u32;
    int32_t i32, j32;
    int16_t i16;
    uint16_t u16;
    uint8_t gd, ed;
    uint8_t wback, wb1, wb2, gb1, gb2;
    int fixedaddress;
    MAYUSE(u16);
    MAYUSE(u8);
    MAYUSE(j32);

    while((opcode==0xF2) || (opcode==0xF3)) {
        rep = opcode-0xF1;
        opcode = F8;
    }
    // REX prefix before the F0 are ignored
    rex.rex = 0;
    while(opcode>=0x40 && opcode<=0x4f) {
        rex.rex = opcode;
        opcode = F8;
    }

    switch(opcode) {

        case 0x0F:
            nextop = F8;
            switch(nextop) {

                case 0xB1:
                    INST_NAME("LOCK CMPXCHG Ed, Gd");
                    SETFLAGS(X_ALL, SF_SET);
                    nextop = F8;
                    GETGD;
                    if(MODREG) {
                        ed = xRAX+(nextop&7)+(rex.b<<3);
                        wback = 0;
                        CMPSxw_REG(xRAX, ed);
                        B_MARK(cNE);
                        UFLAG_IF {emit_cmp32(dyn, ninst, rex, xRAX, ed, x3, x4, x5);}
                        MOVxw_REG(ed, gd);
                        B_MARK_nocond;
                    } else {
                        addr = geted(dyn, addr, ninst, nextop, &wback, x2, &fixedaddress, 0, 0, rex, 0, 0);
                        TSTx_mask(wback, 1, 0, 1+rex.w);    // mask=3 or 7
                        B_MARK3(cNE);
                        // Aligned version
                        MARKLOCK;
                        LDXRxw(x1, wback);
                        ed = x1;
                        CMPSxw_REG(xRAX, ed);
                        B_MARK(cNE);
                        // EAX == Ed
                        STXRxw(x4, gd, wback);
                        CBNZx_MARKLOCK(x4);
                        UFLAG_IF {emit_cmp32(dyn, ninst, rex, xRAX, ed, x3, x4, x5);}
                        // done
                        B_MARK_nocond;
                        // Unaligned version
                        MARK3;
                        LDRxw_U12(x1, wback, 0);
                        LDXRB(x3, wback); // dummy read, to arm the write...
                        ed = x1;
                        CMPSxw_REG(xRAX, ed);
                        B_MARK(cNE);
                        // EAX == Ed
                        STXRB(x4, gd, wback);
                        CBNZx_MARK3(x4);
                        STRxw_U12(gd, wback, 0);
                        UFLAG_IF {emit_cmp32(dyn, ninst, rex, xRAX, ed, x3, x4, x5);}
                        B_MARK_nocond;
                    }
                    MARK;
                    // EAX != Ed
                    UFLAG_IF {emit_cmp32(dyn, ninst, rex, xRAX, ed, x3, x4, x5);}
                    MOVxw_REG(xRAX, ed);
                    break;

                default:
                    DEFAULT;
            }
            break;
                    
        default:
            DEFAULT;
    }
    return addr;
}

