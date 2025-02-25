#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "mima.h"
#include "mima_compiler.h"
#include "log.h"

const char* delimiter = " \n\r";

uint32_t labels_count = 0;
uint32_t labels_capacity = INITIAL_LABEL_CAPACITY;

mima_bool mima_string_to_number(const char *string, uint32_t *number)
{
    *number = -1;

    // check for hex
    if (strncmp(string, "0x", 2) == 0)
    {
        // parse hex
        *number = strtol(string, NULL, 0);
        return mima_true;
    }
    else if (string[0] >= '0' && string[0] <= '9')
    {
        // parse decimal integer
        *number = strtol(string, NULL, 10);
        return mima_true;
    }

    return mima_false;
}

mima_bool mima_string_starts_with_insensitive(const char *string, const char* prefix)
{
    int prefix_char;

    while ((prefix_char = (int)*prefix++) != 0)
    {
        int string_char = (int)*string++;

        if ((string_char == 0) || (tolower(prefix_char) != tolower(string_char)))
        {
            return mima_false;
        }
    }

    return mima_true;
}

mima_bool mima_string_to_op_code(const char *op_code_string, uint32_t *op_code)
{
    uint32_t op_code_result = -1;

    // thank god, this was done in sublime!
    if (mima_string_starts_with_insensitive(op_code_string, "and"))
    {
        op_code_result = AND;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "add"))
    {
        op_code_result = ADD;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "or"))
    {
        op_code_result = OR ;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "xor"))
    {
        op_code_result = XOR;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "ldv"))
    {
        op_code_result = LDV;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "stv"))
    {
        op_code_result = STV;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "ldc"))
    {
        op_code_result = LDC;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "jmp"))
    {
        op_code_result = JMP;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "jmn"))
    {
        op_code_result = JMN;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "eql"))
    {
        op_code_result = EQL;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "hlt"))
    {
        op_code_result = HLT;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "not"))
    {
        op_code_result = NOT;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "rar"))
    {
        op_code_result = RAR;
    }
    else if (mima_string_starts_with_insensitive(op_code_string, "rrn"))
    {
        op_code_result = RRN;
    }

    if (op_code_result == -1)
    {
        return mima_false;
    }

    // Only assign the opcode if asked for:
    if (op_code != NULL)
    {
        *op_code = op_code_result;
    }

    return mima_true;
}

void mima_scan_for_labels(FILE *file)
{
    // This function ignores all syntactical errors and does not log anything.
    // All those diagnostics are applied inside "mima_compile_file()".
    // Here, we will only look for labels.
    // But labels are placed at memory addresses.
    // As a consequence, we still have to discriminate between whitespace lines / comments and instructions, though.

    char line[256];
    size_t line_number = 0;
    size_t memory_address = 0;
    while(fgets(line, sizeof(line), file))
    {
        line_number++;
        char *string = strtok(line, " \r\n");

        if (string == NULL)
        {
            continue;
        }

        // Check for opcodes:
        if (mima_string_to_op_code(string, NULL))
        {
            // Step over the opcode, but increment the memory address.
            memory_address++;
            continue;
        }

        // Check for labels -> safe for later and remeber line number aka address
        if (string[0] == ':')
        {
            log_trace("Line %03zu: %3s for address 0x%08x", line_number, &string[1], memory_address);
            mima_push_label(&string[1], memory_address, line_number);
            continue;
        }

        // Ignore everything else here.
    }

    log_trace("Found %zu label(s) while scanning the input file.", labels_count);
}

mima_bool mima_compile_file(mima_t *mima, const char *file_name)
{
    FILE *file = fopen(file_name, "r");

    if (!file)
    {
        log_error("Failed to open source code file: %s :(", file_name);
        return mima_false;
    }

    log_info("Compiling %s ...", file_name);

    // First, scan the file for labels.
    // This two-pass approach allows us to use them without forward declaration.
    mima_scan_for_labels(file);
    fseek(file, 0, SEEK_SET);

    char line[256];
    size_t line_number = 0;
    size_t memory_address = 0;
    size_t error = 0;
    while(fgets(line, sizeof(line), file))
    {
        line_number++;

        char *string1 = NULL;
        char *string2 = NULL;

        string1 = strtok(line, " \r\n");

        if (string1 == NULL)
        {
            log_warn("Line %03zu: Found nothing useful in \"%s\"", line_number, line);
            error++;
            continue;
        }

        // classify first string, possible things:
        // op code          like: ADD AND OR ...
        // label            like: :Label1, :START, :loop (will be ignored, we have already collected them while scanning) ...
        // address + value  like: 0xF1, 0xFA8 ... = define storage
        // breakpoint       like: b or B

        // string1 is op code
        // -> string2 will be value or empty (NOT, HLT, RAR)
        uint32_t op_code;
        if (mima_string_to_op_code(string1, &op_code))
        {
            uint32_t value = 0;

            // parse value if available
            if (op_code != NOT && op_code != HLT && op_code != RAR)
            {
                string2 = strtok(NULL, delimiter);

                if (!mima_string_to_number(string2, &value))
                {
                    // could not parse number string -> is there a label?
                    value = mima_address_for_label(&string2[0], line_number);
                }
            }

            mima_register instruction = 0;

            if (!mima_assemble_instruction(&instruction, op_code, value, line_number))
            {
                log_error("Line %03zu: Could not assemble instruction: %s", line_number, line);
                error++;
                continue;
            }

            log_trace("Line %03zu: %3s 0x%08x -> stored at mem[0x%08x]", line_number, mima_get_instruction_name(op_code), value, memory_address);
            mima->memory_unit.memory[memory_address++] = instruction;
            continue;
        }

        // string1 is a label -> safe for later and remeber line number aka address
        if (string1[0] == ':')
        {
            continue;
        }

        // define storage: address + hexnumber
        if (mima_string_to_number(string1, &op_code))
        {
            uint32_t value = 0;

            string2 = strtok(NULL, delimiter);

            if (!mima_string_to_number(string2, &value))
            {
                log_error("Found address at line %d - value should follow, but did not.", line_number);
                error++;
            }

            log_trace("Line %03zu: Define mem[0x%08x] = 0x%08x", line_number, op_code, value);

            // op_code holds the address in this case
            mima->memory_unit.memory[op_code] = value;
            continue;
        }

        // line comment
        if (strncmp(string1, "//", 2) == 0 || string1[0] == '#')
        {
            log_trace("Line %03zu: Ignoring comment \"%s\"...", line_number, string1);
            continue;
        }

        // TODO: Breakpoints

        log_warn("Line %03zu: Ignoring - \"%s\"", line_number, line);
    }

    if (error > 0)
    {
        log_error("Found %d error(s) or warning(s) while compiling.", error);
        log_error("Setting mima RUN flag to false.");
        log_error("Nothing will be executed...");
        mima->control_unit.RUN = mima_false;
    }
    else
    {
        log_info("Compiled without errors.");
    }

    return mima_true;
}

void mima_push_label(const char *label_name, uint32_t address, size_t line)
{
    if (labels_count + 1 > labels_capacity)
    {
        labels_capacity *= 2; // double the size
        mima_labels = realloc(mima_labels, sizeof(mima_label) * labels_capacity);

        if (!mima_labels)
        {
            log_error("Line %03zu: Could not realloc memory for labels.", line);
        }
    }

    if (strlen(label_name) > 31)
    {
        log_error("Line %03zu: Label size is limited by 32 chars.", line);
    }

    strncpy(mima_labels[labels_count].label_name, label_name, 31);
    mima_labels[labels_count].address = address;

    labels_count++;
}

uint32_t mima_address_for_label(const char *label_name, size_t line)
{
    for (int i = 0; i < labels_count; ++i)
    {
        log_trace("Line %03zu: Searching for Label %s == %s", line, label_name, mima_labels[i].label_name);

        if (strcmp(mima_labels[i].label_name, label_name) == 0)
        {
            return mima_labels[i].address;
        }
    }

    log_error("Line %03zu: Could not find your label: %s", line, label_name);
    return -1;
}

mima_bool mima_assemble_instruction(mima_register *instruction, uint32_t op_code, uint32_t value, size_t line)
{
    if (op_code < 0 || op_code > 0xFF)
    {
        log_error("Line %03zu: Invalid op code %d", line, op_code);
        return mima_false;
    }

    // extended instruction, no value added
    if (op_code >= 0xF0)
    {
        *instruction = (op_code << 24) | (value & 0x00FFFFFF);
        return mima_true;
    }

    if (value < 0 || value > 0x0FFFFFFF)
    {
        log_error("Line %03zu: Invalid value %d", line, op_code);
        return mima_false;
    }

    *instruction = (op_code << 28) | (value & 0x0FFFFFFF);
    return mima_true;
}
