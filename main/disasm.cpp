#include "disasm.h"
#include <stdarg.h>

typedef enum INSTSETOP_t
{
    INSTSETOP_UNDEF,
    INSTSETOP_MAIN,
    INSTSETOP_SPECIAL,
    INSTSETOP_REGIMM,
    INSTSETOP_COP0,
    INSTSETOP_COP1,
    //COP2‚Íundefinedˆµ‚¢‚Å
} INSTSETOP;

static INSTSETOP OpInstructionSet[1 << 6] = {
    //bit31-26
    INSTSETOP_SPECIAL, INSTSETOP_REGIMM, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_COP0, INSTSETOP_COP1, INSTSETOP_UNDEF, INSTSETOP_UNDEF,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_UNDEF, INSTSETOP_UNDEF, INSTSETOP_UNDEF, INSTSETOP_UNDEF,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_UNDEF, INSTSETOP_UNDEF,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_UNDEF, INSTSETOP_MAIN,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_UNDEF, INSTSETOP_UNDEF,
    INSTSETOP_MAIN, INSTSETOP_MAIN, INSTSETOP_UNDEF, INSTSETOP_MAIN,
};
static INST MainInstruction[1 << 6] = {
    INST_UNDEF, INST_UNDEF, INST_J, INST_JAL, INST_BEQ, INST_BNE, INST_BLEZ, INST_BGTZ,
    INST_ADDI, INST_ADDIU, INST_SLTI, INST_SLTIU, INST_ANDI, INST_ORI, INST_XORI, INST_LUI,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_BEQL, INST_BNEL, INST_BLEZL, INST_BGTZL,
    INST_DADDI, INST_DADDIU, INST_LDL, INST_LDR, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_LB, INST_LH, INST_LWL, INST_LW, INST_LBU, INST_LHU, INST_LWR, INST_LWU,
    INST_SB, INST_SH, INST_SWL, INST_SW, INST_SDL, INST_SDR, INST_SWR, INST_CACHE,
    INST_LL, INST_LWC1, INST_UNDEF, INST_UNDEF, INST_LLD, INST_LDC1, INST_UNDEF, INST_LD,
    INST_SC, INST_SWC1, INST_UNDEF, INST_UNDEF, INST_SCD, INST_SDC1, INST_UNDEF, INST_SD,
};
static INST SpecialInstruction[1 << 6] = {
    INST_SLL, INST_UNDEF, INST_SRL, INST_SRA, INST_SLLV, INST_UNDEF, INST_SRLV, INST_SRAV,
    INST_JR, INST_JALR, INST_UNDEF, INST_UNDEF, INST_SYSCALL, INST_BREAK, INST_UNDEF, INST_SYNC,
    INST_MFHI, INST_MTHI, INST_MFLO, INST_MTLO, INST_DSLLV, INST_UNDEF, INST_DSRLV, INST_DSRAV,
    INST_MULT, INST_MULTU, INST_DIV, INST_DIVU, INST_DMULT, INST_DMULTU, INST_DDIV, INST_DDIVU,
    INST_ADD, INST_ADDU, INST_SUB, INST_SUBU, INST_AND, INST_OR, INST_XOR, INST_NOR,
    INST_UNDEF, INST_UNDEF, INST_SLT, INST_SLTU, INST_DADD, INST_DADDU, INST_DSUB, INST_DSUBU,
    INST_TGE, INST_TGEU, INST_TLT, INST_TLTU, INST_TEQ, INST_UNDEF, INST_TNE, INST_UNDEF,
    INST_DSLL, INST_UNDEF, INST_DSRL, INST_DSRA, INST_DSLL32, INST_UNDEF, INST_DSRL32, INST_DSRA32,
};
static INST RegimmInstruction[1 << 5] = {
    INST_BLTZ, INST_BGEZ, INST_BLTZL, INST_BGEZL, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_TGEI, INST_TGEIU, INST_TLTI, INST_TLTIU, INST_TEQI, INST_UNDEF, INST_TNEI, INST_UNDEF,
    INST_BLTZAL, INST_BGEZAL, INST_BLTZALL, INST_BGEZALL, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};

typedef enum INSTSETCOP0_t
{
    INSTSETCOP0_UNDEF,
    INSTSETCOP0_MAIN,
    INSTSETCOP0_BC0,
    INSTSETCOP0_TLB,
} INSTSETCOP0;

static INSTSETCOP0 Cop0InstructionSet[1 << 5] = {
    INSTSETCOP0_MAIN, INSTSETCOP0_MAIN, INSTSETCOP0_MAIN, INSTSETCOP0_UNDEF,
    INSTSETCOP0_MAIN, INSTSETCOP0_MAIN, INSTSETCOP0_MAIN, INSTSETCOP0_UNDEF,
    INSTSETCOP0_BC0, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
    INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
    INSTSETCOP0_TLB, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
    INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
    INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
    INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF, INSTSETCOP0_UNDEF,
};
static INST Cop0Instruction[1 << 5] = {
    //CFC0,CTC0‚ª•s–¾‚¾‚©‚çUNDEF‚É‚µ‚Æ‚­(ã‚Ì‚ÍMAIN‚Ì‚Ü‚Ü)
    INST_MFC0, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_MTC0, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};
static INST Bc0Instruction[1 << 2] = {
    //‘S•”•s–¾‚¾‚©‚çUNDEF‚É‚µ‚Æ‚­
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF
};
static INST TlbInstruction[1 << 6] = {
    INST_UNDEF, INST_TLBR, INST_TLBWI, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_TLBWR, INST_UNDEF,
    INST_TLBP, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_ERET, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};

typedef enum INSTSETCOP1_t
{
    INSTSETCOP1_UNDEF,
    INSTSETCOP1_MAIN,
    INSTSETCOP1_BC1,
    INSTSETCOP1_S,
    INSTSETCOP1_D,
    INSTSETCOP1_W,
    INSTSETCOP1_L,
} INSTSETCOP1;

static INSTSETCOP1 Cop1InstructionSet[1 << 5] = {
    INSTSETCOP1_MAIN, INSTSETCOP1_MAIN, INSTSETCOP1_MAIN, INSTSETCOP1_UNDEF,
    INSTSETCOP1_MAIN, INSTSETCOP1_MAIN, INSTSETCOP1_MAIN, INSTSETCOP1_UNDEF,
    INSTSETCOP1_BC1, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
    INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
    INSTSETCOP1_S, INSTSETCOP1_D, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
    INSTSETCOP1_W, INSTSETCOP1_L, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
    INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
    INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF, INSTSETCOP1_UNDEF,
};
static INST Cop1Instruction[1 << 5] = {
    INST_MFC1, INST_DMFC1, INST_CFC1, INST_UNDEF,
    INST_MTC1, INST_DMTC1, INST_CTC1, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};
static INST Bc1Instruction[1 << 2] = {
    INST_BC1F, INST_BC1T, INST_BC1FL, INST_BC1TL,
};
static INST SInstruction[1 << 6] = {
    INST_ADD_S, INST_SUB_S, INST_MUL_S, INST_DIV_S, INST_SQRT_S, INST_ABS_S, INST_MOV_S, INST_NEG_S,
    INST_ROUND_L_S, INST_TRUNC_L_S, INST_CEIL_L_S, INST_FLOOR_L_S, INST_ROUND_W_S, INST_TRUNC_W_S, INST_CEIL_W_S,
    INST_FLOOR_W_S,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_CVT_D_S, INST_UNDEF, INST_UNDEF, INST_CVT_W_S, INST_CVT_L_S, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_C_F_S, INST_C_UN_S, INST_C_EQ_S, INST_C_UEQ_S, INST_C_OLT_S, INST_C_ULT_S, INST_C_OLE_S, INST_C_ULE_S,
    INST_C_SF_S, INST_C_NGLE_S, INST_C_SEQ_S, INST_C_NGL_S, INST_C_LT_S, INST_C_NGE_S, INST_C_LE_S, INST_C_NGT_S,
};
static INST DInstruction[1 << 6] = {
    INST_ADD_D, INST_SUB_D, INST_MUL_D, INST_DIV_D, INST_SQRT_D, INST_ABS_D, INST_MOV_D, INST_NEG_D,
    INST_ROUND_L_D, INST_TRUNC_L_D, INST_CEIL_L_D, INST_FLOOR_L_D, INST_ROUND_W_D, INST_TRUNC_W_D, INST_CEIL_W_D,
    INST_FLOOR_W_D,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_CVT_S_D, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_CVT_W_D, INST_CVT_L_D, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_C_F_D, INST_C_UN_D, INST_C_EQ_D, INST_C_UEQ_D, INST_C_OLT_D, INST_C_ULT_D, INST_C_OLE_D, INST_C_ULE_D,
    INST_C_SF_D, INST_C_NGLE_D, INST_C_SEQ_D, INST_C_NGL_D, INST_C_LT_D, INST_C_NGE_D, INST_C_LE_D, INST_C_NGT_D,
};
static INST WInstruction[1 << 6] = {
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_CVT_S_W, INST_CVT_D_W, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};
static INST LInstruction[1 << 6] = {
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_CVT_S_L, INST_CVT_D_L, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
    INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF, INST_UNDEF,
};

INST GetInstruction(r4300word w)
{
    switch (OpInstructionSet[w >> 26])
    {
    case INSTSETOP_UNDEF:
        return INST_UNDEF;
    case INSTSETOP_MAIN:
        return MainInstruction[w >> 26];
    case INSTSETOP_SPECIAL:
        return SpecialInstruction[w & 0x3F];
    case INSTSETOP_REGIMM:
        return RegimmInstruction[w >> 16 & 0x1F];
    case INSTSETOP_COP0:
        switch (Cop0InstructionSet[w >> 21 & 0x1F])
        {
        case INSTSETCOP0_UNDEF:
            return INST_UNDEF;
        case INSTSETCOP0_MAIN:
            return Cop0Instruction[w >> 21 & 0x1F];
        case INSTSETCOP0_BC0:
            return Bc0Instruction[w >> 16 & 0x3];
        case INSTSETCOP0_TLB:
            return TlbInstruction[w & 0x3F];
        }
    case INSTSETOP_COP1:
        switch (Cop1InstructionSet[w >> 21 & 0x1F])
        {
        case INSTSETCOP1_UNDEF:
            return INST_UNDEF;
        case INSTSETCOP1_MAIN:
            return Cop1Instruction[w >> 21 & 0x1F];
        case INSTSETCOP1_BC1:
            return Bc1Instruction[w >> 16 & 0x3];
        case INSTSETCOP1_S:
            return SInstruction[w & 0x3F];
        case INSTSETCOP1_D:
            return DInstruction[w & 0x3F];
        case INSTSETCOP1_W:
            return WInstruction[w & 0x3F];
        case INSTSETCOP1_L:
            return LInstruction[w & 0x3F];
        }
    }
    return INST_UNDEF;
}

const INSTFMT InstFormat[INST_COUNT] = {
    INSTF_NONE,

    INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR,
    INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR,
    INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR, INSTF_ADDRR,
    INSTF_ADDRR, INSTF_ADDRW, INSTF_ADDRW, INSTF_ADDRW,
    INSTF_ADDRW, INSTF_ADDRW, INSTF_ADDRW, INSTF_ADDRW,
    INSTF_ADDRW, INSTF_ADDRW, INSTF_ADDRW, INSTF_NONE,

    INSTF_R3, INSTF_ISIGN, INSTF_ISIGN, INSTF_R3,
    INSTF_R3, INSTF_IUNSIGN, INSTF_R3, INSTF_ISIGN,
    INSTF_ISIGN, INSTF_R3, INSTF_R2, INSTF_R2,
    INSTF_R2, INSTF_R2, INSTF_R2, INSTF_R2,
    INSTF_SA, INSTF_SA, INSTF_R3, INSTF_SA,
    INSTF_SA, INSTF_R3, INSTF_SA, INSTF_SA,
    INSTF_R3, INSTF_R3, INSTF_R3, INSTF_LUI,
    INSTF_R1, INSTF_R1, INSTF_R1, INSTF_R1,
    INSTF_R2, INSTF_R2, INSTF_R3, INSTF_R3,
    INSTF_IUNSIGN, INSTF_SA, INSTF_R3, INSTF_R3,
    INSTF_ISIGN, INSTF_IUNSIGN, INSTF_R3, INSTF_SA,
    INSTF_R3, INSTF_SA, INSTF_R3, INSTF_R3,
    INSTF_R3, INSTF_R3, INSTF_IUNSIGN,

    INSTF_2BRANCH, INSTF_2BRANCH, INSTF_1BRANCH, INSTF_1BRANCH,
    INSTF_1BRANCH, INSTF_1BRANCH, INSTF_1BRANCH, INSTF_1BRANCH,
    INSTF_1BRANCH, INSTF_1BRANCH, INSTF_1BRANCH, INSTF_1BRANCH,
    INSTF_1BRANCH, INSTF_1BRANCH, INSTF_2BRANCH, INSTF_2BRANCH,
    INSTF_J, INSTF_J, INSTF_R2, INSTF_JR,

    INSTF_NONE, INSTF_NONE,

    INSTF_LFR, INSTF_LFR, INSTF_LFW, INSTF_LFW,

    //‚Æ‚è‚ ‚¦‚¸Aƒgƒ‰ƒbƒvŒn‚Í‚æ‚­‚í‚©‚ç‚ñ‚Ì‚Å
    INSTF_NONE, INSTF_NONE, INSTF_NONE, INSTF_NONE,
    INSTF_NONE, INSTF_NONE, INSTF_NONE, INSTF_NONE,
    INSTF_NONE, INSTF_NONE, INSTF_NONE, INSTF_NONE,
    INSTF_NONE, INSTF_NONE,
    INSTF_MFC0, INSTF_MTC0, INSTF_NONE, INSTF_NONE,
    INSTF_NONE, INSTF_NONE,

    INSTF_MFC1, INSTF_MFC1, INSTF_MTC1, INSTF_MTC1,
    INSTF_MTC1, INSTF_MTC1, INSTF_0BRANCH, INSTF_0BRANCH, //BC1F‚Æ‚©‚æ‚­‚í‚©‚Á‚Ä‚È‚¢
    INSTF_0BRANCH, INSTF_0BRANCH,

    INSTF_R3F, INSTF_R3F, INSTF_R3F, INSTF_R3F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F,

    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,


    INSTF_R3F, INSTF_R3F, INSTF_R3F, INSTF_R3F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,
    INSTF_R2F, INSTF_R2F, INSTF_R2F,

    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,
    INSTF_C, INSTF_C, INSTF_C, INSTF_C,

    INSTF_R2F, INSTF_R2F, INSTF_R2F, INSTF_R2F,

};

const char* const OpecodeName[INST_COUNT] = {
    "undef",

    "lb", "lbu", "ld", "ldl",
    "ldr", "lh", "lhu", "ll",
    "lld", "lw", "lwl", "lwr",
    "lwu", "sb", "sc", "scd",
    "sd", "sdl", "sdr", "sh",
    "sw", "swl", "swr", "sync",

    "add", "addi", "addiu", "addu",
    "and", "andi", "dadd", "daddi",
    "daddiu", "daddu", "ddiv", "ddivu",
    "div", "divu", "dmult", "dmultu",
    "dsll", "dsll32", "dsllv", "dsra",
    "dsra32", "dsrav", "dsrl", "dsrl32",
    "dsrlv", "dsub", "dsubu", "lui",
    "mfhi", "mflo", "mthi", "mtlo",
    "mult", "multu", "nor", "or",
    "ori", "sll", "sllv", "slt",
    "slti", "sltiu", "sltu", "sra",
    "srav", "srl", "srlv", "sub",
    "subu", "xor", "xori",

    "beq", "beql", "bgez", "bgezal",
    "bgezall", "bgezl", "bgtz", "bgtzl",
    "blez", "blezl", "bltz", "bltzal",
    "bltzall", "bltzl", "bne", "bnel",
    "j", "jal", "jalr", "jr",

    "break", "syscall",

    "lwc1", "ldc1", "swc1", "sdc1",

    "teq", "teqi", "tge", "tgei",
    "tgeiu", "tgeu", "tlt", "tlti",
    "tltiu", "tltu", "tne", "tnei",
    "cache", "eret",
    "mfc0", "mtc0", "tlbp", "tlbr",
    "tlbwi", "tlbwr",

    "mfc1", "dmfc1", "cfc1", "mtc1",
    "dmtc1", "ctc1", "bc1f", "bc1t",
    "bc1fl", "bc1tl",

    "add.s", "sub.s", "mul.s", "div.s",
    "sqrt.s", "abs.s", "mov.s", "neg.s",
    "round.l.s", "trunc.l.s", "ceil.l.s", "floor.l.s",
    "round.w.s", "trunc.w.s", "ceil.w.s", "floor.w.s",
    "cvt.d.s", "cvt.w.s", "cvt.l.s",

    "c.f.s", "c.un.s", "c.eq.s", "c.ueq.s",
    "c.olt.s", "c.ult.s", "c.ole.s", "c.ule.s",
    "c.sf.s", "c.ngle.s", "c.seq.s", "c.ngl.s",
    "c.lt.s", "c.nge.s", "c.le.s", "c.ngt.s",

    "add.d", "sub.d", "mul.d", "div.d",
    "sqrt.d", "abs.d", "mov.d", "neg.d",
    "round.l.d", "trunc.l.d", "ceil.l.d", "floor.l.d",
    "round.w.d", "trunc.w.d", "ceil.w.d", "floor.w.d",
    "cvt.s.d", "cvt.w.d", "cvt.l.d",

    "c.f.d", "c.un.d", "c.eq.d", "c.ueq.d",
    "c.olt.d", "c.ult.d", "c.ole.d", "c.ule.d",
    "c.sf.d", "c.ngle.d", "c.seq.d", "c.ngl.d",
    "c.lt.d", "c.nge.d", "c.le.d", "c.ngt.d",

    "cvt.s.w", "cvt.d.w", "cvt.s.l", "cvt.d.l",
};

#define BITS(n,length) (w>>(n) & ((1<<(length))-1))

static void noneType(r4300word w, INSTOPERAND* f)
{
}

static void iType(r4300word w, INSTOPERAND* f)
{
    f->i.rs = BITS(21, 5);
    f->i.rt = BITS(16, 5);
    f->i.immediate = BITS(0, 16);
}

static void jType(r4300word w, INSTOPERAND* f)
{
    f->j.inst_index = BITS(0, 26);
}

static void rType(r4300word w, INSTOPERAND* f)
{
    f->r.rs = BITS(21, 5);
    f->r.rt = BITS(16, 5);
    f->r.rd = BITS(11, 5);
    f->r.sa = BITS(6, 5);
}

static void lfType(r4300word w, INSTOPERAND* f)
{
    f->lf.base = BITS(21, 5);
    f->lf.ft = BITS(16, 5);
    f->lf.offset = BITS(0, 16);
}

static void cfType(r4300word w, INSTOPERAND* f)
{
    f->cf.ft = BITS(16, 5);
    f->cf.fs = BITS(11, 5);
    f->cf.fd = BITS(6, 5);
}

const INSTFTYPE InstFormatType[INSTF_COUNT] = {
    INSTFTYPE_NONE,
    INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I, INSTFTYPE_I,
    INSTFTYPE_J,
    INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R, INSTFTYPE_R,
    INSTFTYPE_LF, INSTFTYPE_LF,
    INSTFTYPE_CF, INSTFTYPE_CF, INSTFTYPE_CF
};
void (*InstFormatTypeFunc[INSTFTYPE_COUNT])(r4300word, INSTOPERAND*) = {
    noneType, iType, jType, rType, lfType, cfType
};

void DecodeInstruction(r4300word w, INSTDECODE* d)
{
    INST inst = GetInstruction(w);
    INSTFMT format;
    INSTFTYPE type;
    if (inst == INST_UNDEF)
    {
        inst = GetInstruction(w);
    }
    format = InstFormat[inst];
    type = InstFormatType[format];
    InstFormatTypeFunc[type](w, &d->operand);
    d->inst = inst;
    d->format = format;
    d->type = type;
}

//max-length:9
const char* GetOpecodeString(INSTDECODE* d)
{
    return OpecodeName[d->inst];
}

const char* const CPURegisterName[32] = {
    "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra",
};
const char* const COP0RegisterName[32] = {
    "index", "random", "entrylo0", "entrylo1",
    "context", "pagemask", "wired", "reserved07",
    "badvaddr", "count", "entryhi", "compare",
    "status", "cause", "epc", "previd",
    "config", "lladdr", "watchlo", "watchhi",
    "xcontext", "reserved21", "reserved22", "reserved23",
    "reserved24", "reserved25", "perr", "cacheerr",
    "taglo", "taghi", "errorepc", "reserved1f"
};

static char* sfmt(char* b, const char* f, ...)
{
#define HEX4()			q[0] = x[n>>12&0xF];\
			q[1] = x[n>>8&0xF];\
			q[2] = x[n>>4&0xF];\
			q[3] = x[n&0xF];\
			q+=4;

    va_list v;
    const char* p;
    char* q = b;
    const char* const x = "0123456789abcdef";
    va_start(v, f);
    for (p = f; *p; p++)
    {
        char c = *p;
        switch (c)
        {
        case 'r':
            {
                const char* l;
                for (l = CPURegisterName[va_arg(v, int)]; *l; l++)
                {
                    *(q++) = *l;
                }
                break;
            }
        case 'c':
            {
                const char* l;
                for (l = COP0RegisterName[va_arg(v, int)]; *l; l++)
                {
                    *(q++) = *l;
                }
                break;
            }
        case 'f':
            {
                int f = va_arg(v, int);
                q[0] = 'f';
                //decimal
                q[1] = x[f / 10];
                q[2] = x[f % 10];
                q += 3;
                break;
            }
        case 'u':
            {
                r4300half n = va_arg(v, r4300half);
                HEX4();
                break;
            }
        case 's':
            {
                r4300half n = va_arg(v, r4300half);
                if (n < 0x8000)
                {
                    *q = '+';
                }
                else
                {
                    *q = '+';
                    //				n = -n;
                }
                q++;
                HEX4();
                break;
            }
        case 'p':
            {
                r4300word m = va_arg(v, r4300word);
                r4300half n = m >> 16;
                HEX4();
                n = m & 0xFFFF;
                HEX4();
                break;
            }
        case 'a':
            {
                r4300byte n = va_arg(v, r4300byte);
                q[0] = x[n >> 4];
                q[1] = x[n & 0xF];
                q += 2;
                break;
            }
        default:
            *(q++) = c;
            break;
        }
    }
    va_end(v);
    *q = '\0';
    return q;
#undef HEX4
}

//max-length: 16+1
char* GetOperandString(char* buf, INSTDECODE* d, r4300word pc)
{
#define BRANCH (pc+4+((r4300wordsigned)(r4300halfsigned)o->i.immediate << 2))
    INSTOPERAND* o = &d->operand;
    //max-length: r:2, c:10, f:3, s:5, u:4, p:8, a:2, *:1
    switch (d->format)
    {
    default:
    case INSTF_NONE:
        return sfmt(buf, ""); //max-length: 0
    case INSTF_LUI:
        return sfmt(buf, "r, s", o->i.rt, o->i.immediate); //9
    case INSTF_ISIGN:
        return sfmt(buf, "r, r, s", o->i.rt, o->i.rs, o->i.immediate); //13
    case INSTF_IUNSIGN:
        return sfmt(buf, "r, r, u", o->i.rt, o->i.rs, o->i.immediate); //12
    case INSTF_0BRANCH:
        return sfmt(buf, "p", BRANCH); //8
    case INSTF_1BRANCH:
        return sfmt(buf, "r, p", o->i.rs, BRANCH); //12
    case INSTF_2BRANCH:
        return sfmt(buf, "r, r, p", o->i.rs, o->i.rt, BRANCH); //16
    case INSTF_ADDRR:
    case INSTF_ADDRW:
        return sfmt(buf, "r, s(r)", o->i.rt, o->i.immediate, o->i.rs); //13
    case INSTF_JR:
        return sfmt(buf, "r", o->i.rs);
    case INSTF_J:
        return sfmt(buf, "p", (pc & 0xF0000000) | (o->j.inst_index << 2)); //8
    case INSTF_MTC0:
    case INSTF_MFC0:
        return sfmt(buf, "r, c", o->r.rt, o->r.rd); //14
    case INSTF_MTC1:
    case INSTF_MFC1:
        return sfmt(buf, "r, f", o->r.rt, o->r.rd); //7
    case INSTF_R1:
        return sfmt(buf, "r", o->r.rd); //2
    case INSTF_R2:
        return sfmt(buf, "r, r", o->r.rs, o->r.rt); //6
    case INSTF_R3:
        return sfmt(buf, "r, r, r", o->r.rd, o->r.rs, o->r.rt); //10
    case INSTF_SA:
        return sfmt(buf, "r, r, a", o->r.rd, o->r.rt, o->r.sa); //10
    case INSTF_LFR:
    case INSTF_LFW:
        return sfmt(buf, "f, s(r)", o->i.rt, o->i.immediate, o->i.rs); //14
    case INSTF_R2F:
        return sfmt(buf, "f, f", o->cf.fd, o->cf.fs); //8
    case INSTF_C:
        return sfmt(buf, "f, f", o->cf.fs, o->cf.ft); //8
    case INSTF_R3F:
        return sfmt(buf, "f, f, f", o->cf.fd, o->cf.fs, o->cf.ft); //13
    }
#undef BRANCH
}

//max-size:27 = 9(opecode)+1(space)+16(operand)+1(NUL)
char* DisassembleInstruction(char* buf, r4300word w, r4300word pc)
{
    INSTDECODE decode;
    const char* p;
    DecodeInstruction(w, &decode);
    for (p = GetOpecodeString(&decode); *p; p++)
    {
        *(buf++) = *p;
    }
    *(buf++) = ' ';
    return GetOperandString(buf, &decode, pc);
}
