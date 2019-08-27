#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "mima.h"
#include "mima_compiler.h"

mima_t mima_init()
{

    mima_t mima =
    {
        .control_unit = {
            .IR  = 0,
            .IAR = 0,
            .IP  = 0,
            .TRA = 0,
            .RUN = mima_true
        },
        .memory_unit = {
            .SIR = 0,
            .SAR = 0,
            .memory = 0
        },
        .processing_unit = {
            .ONE = 1,
            .ACC = 0,
            .X   = 0,
            .Y   = 0,
            .Z   = 0,
            .MICRO_CYCLE = 1 // 1 - 12 cycles per instruction
        }
    };

    // we allocate mima words aka integers
    mima.memory_unit.memory = malloc(mima_words * sizeof(mima_word));

    if(!mima.memory_unit.memory)
    {
        log_fatal("Could not allocate Mima memory :(\n");
        assert(0);
    }

    mima_labels = malloc(labels_capacity * sizeof(mima_label));

    if (!mima_labels)
    {
        log_fatal("Could not allocate memory for labels :(\n");
        assert(0);
    }

    return mima;
}

void mima_run(mima_t *mima)
{
    log_trace("\n================================\nStarting Mima...\n");
    while(mima->control_unit.RUN == mima_true)
    {
        mima_instruction_step(mima);
        //int c = getchar();
    }
}

mima_bool mima_compile(mima_t *mima, const char *file_name)
{
    return mima_compile_file(mima, file_name);
}

mima_instruction mima_instruction_decode(mima_t *mima)
{

    mima_word mem = mima->memory_unit.memory[mima->memory_unit.SAR];

    mima_instruction instr;

    if(mem >> 28 != 0xF)
    {
        // standard instruction
        instr.op_code   = mem >> 28;
        instr.value     = mem & 0x0FFFFFFF;
        instr.extended  = mima_false;
    }
    else
    {
        // extended instruction
        instr.op_code   = mem >> 24;
        instr.value     = mem & 0x00FFFFFF;
        instr.extended  = mima_true;
    }

    return instr;
}

mima_bool mima_sar_external(mima_t *mima)
{
    if(mima->current_instruction.value >= 0xC000000)
        return mima_true;
    return mima_false;
}

void mima_instruction_step(mima_t *mima)
{
    //FETCH: first 5 cycles are the same for all instructions
    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 1:
        mima->memory_unit.SAR   = mima->control_unit.IAR;
        mima->processing_unit.X = mima->control_unit.IAR;
        break;
    case 2:
        mima->processing_unit.Y = mima->processing_unit.ONE;
        mima->processing_unit.ALU = ADD;
        break;
    case 3:
        mima->processing_unit.Z = mima->processing_unit.X + mima->processing_unit.Y;
        break;
    case 4:
        mima->control_unit.IAR = mima->processing_unit.Z;
        mima->current_instruction = mima_instruction_decode(mima);
        mima->memory_unit.SIR = mima->memory_unit.memory[mima->memory_unit.SAR];
        break;
    case 5:
        mima->control_unit.IR = mima->memory_unit.SIR;
        break;
    default:
    {
        switch(mima->current_instruction.op_code)
        {
        case AND:
        case OR:
        case XOR:
        case ADD:
        case EQL:
            mima_instruction_common(mima);
            break;
        case LDV:
            mima_instruction_LDV(mima);
            break;
        case STV:
            mima_instruction_STV(mima);
            break;
        case LDC:
            mima_instruction_LDC(mima);
            break;
        case HLT:
            mima_instruction_HLT(mima);
            break;
        case JMP:
            mima_instruction_JMP(mima);
            break;
        case JMN:
            mima_instruction_JMN(mima);
            break;
        case NOT:
            mima_instruction_NOT(mima);
            break;
        case RAR:
            mima_instruction_RAR(mima);
            break;
        case RRN:
            mima_instruction_RRN(mima);
            break;
        default:
            log_warn("Invalid instruction - nr.%d - :(\n", mima->current_instruction.op_code);
            assert(0);
        }
    }
    }

    mima->processing_unit.MICRO_CYCLE++;
    if (mima->processing_unit.MICRO_CYCLE > 12)
    {
        mima->processing_unit.MICRO_CYCLE = 1;
    }
}

// ADD, AND, OR, XOR, EQL
void mima_instruction_common(mima_t *mima)
{
    log_trace("Executing %s - %02d", mima_get_instruction_name(mima->current_instruction.op_code), mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        mima->memory_unit.SAR = mima->control_unit.IR & 0x0FFFFFFF;
        break;
    case 7:
        mima->processing_unit.X = mima->processing_unit.ACC;
        break;
    case 8:
        break;
    case 9:
        mima->memory_unit.SIR = mima->memory_unit.memory[mima->memory_unit.SAR];
        break;
    case 10:
        mima->processing_unit.Y = mima->memory_unit.SIR;
        break;
    case 11:
    {
        switch(mima->current_instruction.op_code)
        {
        case AND:
            mima->processing_unit.Z = mima->processing_unit.X & mima->processing_unit.Y;
            break;
        case OR:
            mima->processing_unit.Z = mima->processing_unit.X | mima->processing_unit.Y;
            break;
        case XOR:
            mima->processing_unit.Z = mima->processing_unit.X ^ mima->processing_unit.Y;
            break;
        case ADD:
            mima->processing_unit.Z = mima->processing_unit.X + mima->processing_unit.Y;
            break;
        case EQL:
            mima->processing_unit.Z = mima->processing_unit.X == mima->processing_unit.Y ? -1 : 0;
            break;
        default:
            break;
        }
        break;
    }
    case 12:
        mima->processing_unit.ACC = mima->processing_unit.Z;
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_LDV(mima_t *mima)
{
    log_trace("Executing LDV - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        mima->memory_unit.SAR = mima->control_unit.IR & 0x0FFFFFFF;
        break;
    case 7:
        break;
    case 8:
        break;
    case 9:
        log_trace("Executing LDV - %02d: Loading from memory address 0x%08x", mima->processing_unit.MICRO_CYCLE, mima->memory_unit.SAR);
        mima->memory_unit.SIR = mima->memory_unit.memory[mima->memory_unit.SAR];
        break;
    case 10:
        mima->processing_unit.ACC = mima->memory_unit.SIR;
        break;
    case 11:
        break;
    case 12:
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_STV(mima_t *mima)
{
    log_trace("Executing STV - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        mima->memory_unit.SIR = mima->processing_unit.ACC;
        break;
    case 7:
        mima->memory_unit.SAR = mima->control_unit.IR;
        mima_register address = mima->control_unit.IR & 0x0FFFFFFF;

        log_trace("Executing STV - %02d: Writing 0x%08x to memory address 0x%08x", mima->processing_unit.MICRO_CYCLE, mima->memory_unit.SIR, address);

        // writing to "internal" memory
        if (address < 0xc000000)
        {
            mima->memory_unit.memory[address] = mima->memory_unit.SIR;
            break;
        }
        else
        {
            // writing to IO -> ignoring the  first 4 bits
            if (address == mima_ascii_output)
            {
                printf("%c\n", mima->memory_unit.SIR & 0x0FFFFFFF);
                break;
            }

            if (address == mima_integer_output)
            {
                printf("%d\n", mima->memory_unit.SIR & 0x0FFFFFFF);
                break;
            }

            log_warn("Writing into undefined I/O space. Nothing will happen!");

            break;
        }

    case 8:
        break;
    case 9:
        break;
    case 10:
        break;
    case 11:
        break;
    case 12:
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_HLT(mima_t *mima)
{
    log_trace("Executing HLT - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        break;
    case 7:
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        break;
    case 11:
        break;
    case 12:
        log_trace("Halting mima");
        mima->control_unit.RUN = mima_false;
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_LDC(mima_t *mima)
{
    log_trace("Executing LDC - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        mima->processing_unit.ACC = mima->control_unit.IR & 0x0FFFFFFF;
        break;
    case 7:
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        break;
    case 11:
        break;
    case 12:
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_JMP(mima_t *mima)
{
    log_trace("Executing JMP - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        mima->control_unit.IAR = mima->control_unit.IR & 0x0FFFFFFF;
        break;
    case 7:
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        break;
    case 11:
        break;
    case 12:
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_JMN(mima_t *mima)
{
    log_trace("Executing JMN - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        if((int32_t)mima->processing_unit.ACC < 0)
        {
            log_trace("Executing JMN - %d ACC = %d - Jumping to: 0x%08x", mima->processing_unit.MICRO_CYCLE, mima->processing_unit.ACC, mima->control_unit.IR & 0x0FFFFFFF);
            mima->control_unit.IAR = mima->control_unit.IR & 0x0FFFFFFF;
        }
        break;
    case 7:
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        break;
    case 11:
        break;
    case 12:
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_NOT(mima_t *mima)
{
    log_trace("Executing NOT - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        break;
    case 7:
        mima->processing_unit.X = mima->processing_unit.ACC;
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        mima->processing_unit.ALU = NOT;
        // SET ACC -> NOT
        break;
    case 11:
        mima->processing_unit.Z = ~mima->processing_unit.X;
        break;
    case 12:
        mima->processing_unit.ACC = mima->processing_unit.Z;
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_RAR(mima_t *mima)
{
    log_trace("Executing RAR - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        break;
    case 7:
        mima->processing_unit.X = mima->processing_unit.ACC;
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        mima->processing_unit.Y = mima->processing_unit.ONE;
        mima->processing_unit.ALU = RAR;
        // SET ACC -> RAR
        break;
    case 11:
    {
        int32_t shifted = mima->processing_unit.Z >> mima->processing_unit.Y;
        int32_t rotated = mima->processing_unit.Z << (32 - mima->processing_unit.Y);
        int32_t combined = shifted | rotated;
        mima->processing_unit.Z = combined;
        break;
    }
    case 12:
        mima->processing_unit.ACC = mima->processing_unit.Z;
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_instruction_RRN(mima_t *mima)
{
    log_trace("Executing RRN - %02d", mima->processing_unit.MICRO_CYCLE);

    switch(mima->processing_unit.MICRO_CYCLE)
    {
    case 6:
        break;
    case 7:
        mima->processing_unit.X = mima->processing_unit.ACC;
        break;
    case 8:
        break;
    case 9:
        break;
    case 10:
        mima->processing_unit.Y = mima->control_unit.IR & 0x00FFFFFF;
        mima->processing_unit.ALU = RRN;
        // SET ACC -> RRN
        break;
    case 11:
    {
        int32_t shifted = mima->processing_unit.Z >> mima->processing_unit.Y;
        int32_t rotated = mima->processing_unit.Z << (32 - mima->processing_unit.Y);
        int32_t combined = shifted | rotated;
        mima->processing_unit.Z = combined;
        break;
    }
    case 12:
        mima->processing_unit.ACC = mima->processing_unit.Z;
        break;
    default:
        log_warn("Invalid micro cycle. Must be between 6-12, was %d :(\n", mima->processing_unit.MICRO_CYCLE);
        assert(0);
    }
}

void mima_print_memory_at(mima_t *mima, mima_register address)
{
    if (address < 0 || address > mima_words - 1)
    {
        log_warn("Invalid address %d\n", address);
        return;
    }

    for (int i = 0; address + i < mima_words - 1 && i < 10; ++i)
    {
        printf("mem[0x%08x] = 0x%08x\n", address + i, mima->memory_unit.memory[address + i]);
    }
}

void mima_print_memory_unit_state(mima_t *mima)
{
    printf("\n");
    printf("=======MEMORY UNIT=======\n");
    printf("SIR = 0x%08x\n", mima->memory_unit.SIR);
    printf("SAR = 0x%08x\n", mima->memory_unit.SAR);
    printf("=========================\n");
}

void mima_print_control_unit_state(mima_t *mima)
{
    printf("\n");
    printf("=======CONTROL UNIT======\n");
    printf("IR  = 0x%08x\n", mima->control_unit.IR);
    printf("IAR = 0x%08x\n", mima->control_unit.IAR);
    printf("IP  = 0x%08x\n", mima->control_unit.IP);
    printf("TRA = %s\n", mima->control_unit.TRA ? "true" : "false");
    printf("RUN = %s\n", mima->control_unit.RUN ? "true" : "false");
    printf("=========================\n");
}

void mima_print_processing_unit_state(mima_t *mima)
{
    printf("\n");
    printf("=======PROC UNIT=========\n");
    printf("ACC = 0x%08x\n", mima->processing_unit.ACC);
    printf("X   = 0x%08x\n", mima->processing_unit.X);
    printf("Y   = 0x%08x\n", mima->processing_unit.Y);
    printf("Z   = 0x%08x\n", mima->processing_unit.Z);
    printf("MICRO_CYCLE = %02d\n", mima->processing_unit.MICRO_CYCLE);
    printf("ALU = %s\n", mima_get_instruction_name(mima->current_instruction.op_code));
    printf("=========================\n");
}

void mima_print_state(mima_t *mima)
{
    mima_print_processing_unit_state(mima);
    mima_print_control_unit_state(mima);
    mima_print_memory_unit_state(mima);
}

const char *mima_get_instruction_name(mima_instruction_type op_code)
{
    switch(op_code)
    {
    case ADD:
        return "ADD";
    case AND:
        return "AND";
    case OR:
        return "OR";
    case XOR:
        return "XOR";
    case LDV:
        return "LDV";
    case STV:
        return "STV";
    case LDC:
        return "LDC";
    case JMP:
        return "JMP";
    case JMN:
        return "JMN";
    case EQL:
        return "EQL";
    case HLT:
        return "HLT";
    case NOT:
        return "NOT";
    case RAR:
        return "RAR";
    case RRN:
        return "RRN";
    }
    return "INVALID";
}

void mima_delete(mima_t *mima)
{
    free(mima->memory_unit.memory);
    free(mima_labels);
}


