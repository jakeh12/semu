#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define MEMORY_DEPTH 256

enum states
{
    S_00_PC_TO_MEM_ADDR,
    S_01_MEM_DATA_TO_MAR,
    S_02_MAR_TO_MEM_ADDR,
    S_03_MEM_DATA_TO_A,
    S_04_PC_TO_MEM_ADDR,
    S_05_MEM_DATA_TO_MAR,
    S_06_MAR_TO_MEM_ADDR,
    S_07_MEM_DATA_TO_B,
    S_08_B_MINUS_A_TO_MEM_DATA,
    S_09_PC_TO_MEM_ADDR,
    S_10_MEM_DATA_TO_MAR,
    S_11_BRANCH_TO_MAR_OR_INC_PC
};

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t mar;
    uint64_t pc;
    uint8_t state;
    uint8_t error;
    uint8_t memory[MEMORY_DEPTH];
    int instruction_count;
    int clock_count;
} subleq_t;

void load_program(subleq_t* s, char* path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file %s: %d %s\n", path, errno, strerror(errno));
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    int length = (int) ftell(file);
    if (length > MEMORY_DEPTH)
    {
        fprintf(stderr, "program file %s (%d bytes) is bigger than memory.\nLoading only the first %d bytes.\n", path, length, MEMORY_DEPTH);
        length = MEMORY_DEPTH;
    }
    fseek(file, 0, SEEK_SET);
    int i;
    uint8_t c;
    for (i = 0; i < length; i++)
    {
        fread(&c, 1, 1, file);
        s->memory[i] = (uint8_t) c;
    }
    fclose(file);
}


void reset(subleq_t* s)
{
    s->a = 0;
    s->b = 0;
    s->mar = 0;
    s->pc = 0;
    s->state = 0;
    s->error = 0;
    int i;
    for (i = 0; i < MEMORY_DEPTH; i++)
    {
        s->memory[i] = 0;
    }
    s->instruction_count = 0;
    s->clock_count = 0;
}

void step_clock_cycle(subleq_t* s)
{
    switch (s->state)
    {
        case S_00_PC_TO_MEM_ADDR:
            //s->memory[s->pc];
            s->state = S_01_MEM_DATA_TO_MAR;
            s->clock_count++;
            break;
        case S_01_MEM_DATA_TO_MAR:
            s->mar = s->memory[s->pc];
            s->state = S_02_MAR_TO_MEM_ADDR;
            s->clock_count++;
            break;
        case S_02_MAR_TO_MEM_ADDR:
            //s->memory[s->mar];
            s->state = S_03_MEM_DATA_TO_A;
            s->clock_count++;
            break;
        case S_03_MEM_DATA_TO_A:
            s->a = s->memory[s->mar];
            s->pc = s->pc + 1;
            s->state = S_04_PC_TO_MEM_ADDR;
            s->clock_count++;
            break;
        case S_04_PC_TO_MEM_ADDR:
            //s->memory[s->pc];
            s->state = S_05_MEM_DATA_TO_MAR;
            s->clock_count++;
            break;
        case S_05_MEM_DATA_TO_MAR:
            s->mar = s->memory[s->pc];
            s->state = S_06_MAR_TO_MEM_ADDR;
            s->clock_count++;
            break;
        case S_06_MAR_TO_MEM_ADDR:
            //s->memory[s->mar];
            s->state = S_07_MEM_DATA_TO_B;
            s->clock_count++;
            break;
        case S_07_MEM_DATA_TO_B:
            s->b = s->memory[s->mar];
            s->pc = s->pc + 1;
            s->state = S_08_B_MINUS_A_TO_MEM_DATA;
            s->clock_count++;
            break;
        case S_08_B_MINUS_A_TO_MEM_DATA:
            s->memory[s->mar] = s->b - s->a;
            if (s->mar == MEMORY_DEPTH-1)
            {
                // if write to last memory location, print to stdout
                putchar((char)(s->b - s->a));
            }
            s->state = S_09_PC_TO_MEM_ADDR;
            s->clock_count++;
            break;
        case S_09_PC_TO_MEM_ADDR:
            //s->memory[s->pc];
            s->state = S_10_MEM_DATA_TO_MAR;
            s->clock_count++;
            break;
        case S_10_MEM_DATA_TO_MAR:
            s->mar = s->memory[s->pc];
            s->state = S_11_BRANCH_TO_MAR_OR_INC_PC;
            s->clock_count++;
            break;
        case S_11_BRANCH_TO_MAR_OR_INC_PC:
            if (s->b - s->a <= 0)
            {
                s->pc = s->mar;
            }
            else
            {
                s->pc = s->pc + 1;
            }
            s->state = S_00_PC_TO_MEM_ADDR;
            s->clock_count++;
            s->instruction_count++;
            break;
        default:
            // should never happen
            s->error = 1;
            break;
    }
}

void step_instruction(subleq_t* s)
{
    if (s->state != S_00_PC_TO_MEM_ADDR)
    {
        // the processor is in the middle of executing an instruction, finish it
        while (s->state != S_00_PC_TO_MEM_ADDR)
        {
            step_clock_cycle(s);
        }
    }
    else
    {
        // execute entire instruction
        do
        {
            step_clock_cycle(s);
        }
        while (s->state != S_00_PC_TO_MEM_ADDR);
    }
}



int main(int argc, const char * argv[])
{
    subleq_t* s = (subleq_t*) calloc(1, sizeof(subleq_t));
    reset(s);
    if (argc > 1) {
        load_program(s, (char*) argv[1]);
    }
    
    char buffer[256];
    char op[32];
    int addr = 0;
    int val = 0;
    while (1)
    {
        printf(" > ");
        fgets(buffer, 256, stdin);
        sscanf(buffer, "%s %d %d", op, &addr, &val);
        if (strcmp(op, "si") == 0 || strcmp(op, "s") == 0)
        {
            step_instruction(s);
            printf("pc: 0x%02x\n", (uint8_t)s->pc);
        }
        else if (strcmp(op, "c") == 0)
        {
            int i;
            for (i = 0; i < addr; i++)
            {
                step_instruction(s);
            }
            printf("pc: 0x%02x\n", (uint8_t)s->pc);
        }
        else if (strcmp(op, "sc") == 0)
        {
            step_clock_cycle(s);
            printf("pc: 0x%02x\tstate: %02d\n", (uint8_t)s->pc, s->state);
        }
        else if (strcmp(op, "rm") == 0)
        {
            if (addr <= val)
            {
                int i;
                for (i = addr; addr <= val; addr++)
                {
                    printf("0x%02x: 0x%02x\n", addr, s->memory[addr]);
                }
            }
            else
            {
                printf("0x%02x\n", s->memory[addr]);
            }
        }
        else if (strcmp(op, "wm") == 0)
        {
            printf("0x%02x -> ", s->memory[addr]);
            s->memory[addr] = val;
            printf("0x%02x\n", s->memory[addr]);
        }
        else if (strcmp(op, "ps") == 0)
        {
            printf("instructions: %d\tclock cycles: %d\n", s->instruction_count, s->clock_count);
        }
        else if (strcmp(op, "rr") == 0)
        {
            printf("a:   0x%02x\n", s->a);
            printf("b:   0x%02x\n", s->b);
            printf("mar: 0x%02x\n", s->mar);
            printf("pc:  0x%02x\n", (uint8_t)s->pc);
        }
        else if (strcmp(op, "h") == 0)
        {
            printf("h            -- prints this help page\n"
                   "si   or   s  -- single instruction step\n"
                   "sc           -- single clock cycle step (sub-instruction)\n"
                   "c <x>        -- execute x many instructions\n"
                   "rr           -- print all register contents\n"
                   "rm <x> <<y>> -- read memory address x (or range x to y)\n"
                   "wm <x> <y>   -- write value y into address x\n"
                   "ps           -- print statistics\n"
                   "q            -- quit"
            );
        }
        else if (strcmp(op, "q") == 0)
        {
            exit(0);
        }
        else
        {
            printf("unknown\n");
        }
        buffer[0] = 0;
        op[0] = 0;
        addr = 0;
        val = 0;
    }
}
