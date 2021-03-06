/* 
 * File:   first_pass.c
 * Author: evgeny
 *
 * Created on July 4, 2013, 7:15 PM
 */

#include "first_pass.h"

int data_area[2000] = {};
int opr_area[2000] = {};
int ic = 0, dc = 0;
Symbol *symbol_table[HASH_TAB_SIZE];

/*
 * First pass over the code
 */
void first_pass() {
    sglib_hashed_Symbol_init(symbol_table);
    int error = 0, line_num = 0;

    char line[100];
    while (!feof(target_file)) {
        if (!get_line(line)) {
            continue;
        }

        error = handle_instr(line);
        if (error == 0) {
            continue;
        } else if (error > 0) {
            handle_error(error, line);
        }

        error = handle_operation(line);
        if (error == 0) {
            continue;
        } else if (error > 0) {
            handle_error(error, line);
        }

        line_num++;
    }
    rewind(target_file);
}

/* Get a line to process */
bool get_line(char* line) {
    fgets(line, MAX_LINE_SIZE, target_file);
    /*Trim whitespace*/
    trim_whitespace(line);

    /* Should we process the line? */
    if (is_meaningless_line(line)) {
        return false;
    }
    return true;
}

/* Advance IC for required code length */
int handle_operation(char *line) {
    if (get_opert_type(line) != NONE) {
        if (has_label(line)) {
            add_symbol(get_label_name(line), ic, false, false);
        }
        ic += calc_code_length(line);
        return 0;
    }
    return -1;
}

/* Advance dc for required length */
int handle_instr(char *line) {
    enum Instr instr;
    /* Get instruction from line (if exists)*/
    instr = get_instr(line);
    /* Handle data */
    if (instr == data || instr == string) {
        if (has_label(line)) {
            add_symbol(get_label_name(line), dc, false, true);
        }
        if (instr == string) {
            handle_string_instr(line);
        } else if (instr == data) {
            handle_data_instr(line);
        }
    } else if (instr == entry || instr == extrn) {
        /*Handle extern and entry instructions*/
        if (extrn) {
            add_symbol(get_symbol_name(line), 0, true, false);
        } else {
            add_symbol(get_symbol_name(line), 0, true, true);
        }
    }

    if (instr != NONE) {
        return 0;
    }
    return -1;
}

/* Should we process this line? */
bool is_meaningless_line(char *line) {
    /*Check for empty string*/
    if (is_empty_string(line)) {
        return true;
    }

    /*Is it a comment?*/
    if (starts_with_char(line, ';')) {
        return true;
    }
    return false;
}

/* Does this line contains label? */
bool has_label(char *line) {
    if (get_label_name(line) == NULL) {
        return false;
    }
    return true;
}

void notify_error(char *msg, int line_num) {
    printf("You have an error on line %d:\n", line_num);
    printf("%s\n", msg);
}

/* Get instruction from line */
int get_instr(char *line) {
    char *instr_tok, *garbage_line = copy_line(line);

    /*If has symbol, look in the second token*/
    if (has_label(line)) {
        strtok(garbage_line, " ");
        instr_tok = strtok(NULL, " ");
    } else {
        instr_tok = strtok(garbage_line, " ");
    }

    /* Return appropriate pseudo instruction. Not the prettiest memory operation, but we write C stuff :-/ */
    if (strcmp(instr_tok, ".data") == 0) {
        free(garbage_line);
        return data;
    } else if (strcmp(instr_tok, ".string") == 0) {
        free(garbage_line);
        return string;
    } else if (strcmp(instr_tok, ".entry") == 0) {
        free(garbage_line);
        return entry;
    } else if (strcmp(instr_tok, ".extern") == 0) {
        free(garbage_line);
        return extrn;
    }

    /* Seems like there is no instruction*/
    free(garbage_line);
    return NONE;

}

/* Get IC length for an operation */
int calc_code_length(char *line) {
    enum OpertType opert = get_opert_type(line);

    if (opert == RTS || opert == STOP) {
        return 1;
    }

    bool binary = is_binary_operation(opert);

    if (binary) {
        return get_binary_length(line);
    }

    line = remove_before_space(line);
    return 1 + get_single_operand_info(line, NULL, NULL);


}

/* Do we have two operands? */
bool is_binary_operation(enum OpertType opert) {
    bool binary = (opert == MOV || opert == CMP ||
            opert == ADD || opert == SUB ||
            opert == LEA);
    return binary;
}

/* Calculate IC length for two operands */
int get_binary_length(char *line) {
    line = remove_before_space(line);
    int result = 1;
    Split *split = split_string(line, ',');

    result += get_single_operand_info(split -> head, NULL, NULL);
    result += get_single_operand_info(split -> tail, NULL, NULL);

    free(split);
    return result;
}

/* It returns the L for current opperand. 
 * reg holds the register number and adr adresation method*/
int get_single_operand_info(char *oper, int *reg, int *adr) {
    /* "Elegant" (just like Fiat Multipla) solution to overriding/default args */
    int mock;
    if (reg == NULL) {
        reg = &mock;
    }
    if (adr == NULL) {
        adr = &mock;
    }

    oper = trim_whitespace(oper);
    /* Is it a register? */
    *reg = get_register_code(oper);
    if (*reg != INVALID) {
        *adr = 3;
        return 0;
    }

    /* ...or maybe immediate number? */
    if (starts_with_char(oper, '#')) {
        *adr = 0;
        return 1;
    }
    /* so it should be an index call! */
    int index_type = get_index_type(oper, reg);
    if (index_type == REGISTER) {
        *adr = 2;
        return 1;
    }
    if (index_type == REFERENCE || index_type == IMMEDIATE) {
        *adr = 2;
        return 2;
    }

    *adr = 1;
    return 1;
}

/* Get index type if it is an array operand */
int get_index_type(char *oper, int *reg) {
    int error = 0, mock = 0;
    if (reg == NULL) {
        *reg = mock;
    }
    char *index_expr = get_index_expr(oper, &error);

    if (error == NONE || error == INVALID) {
        return error;
    }

    *reg = get_register_code(index_expr);
    if (*reg != INVALID) {
        return REGISTER;
    }

    if (isdigit(index_expr)) {
        return IMMEDIATE;
    }

    return REFERENCE;

}

/* Parse register code from the operand. Returns INVALID if it's not a register. */
int get_register_code(char *oper) {
    char *copy, *copyp;
    int result = INVALID;

    /* We want to be space tolerant, so copy the string and trim spaces*/
    copy = (char *) malloc(strlen(oper) * sizeof (char));
    strcpy(copy, oper);
    copyp = copy;
    copy = trim_whitespace(copy);

    int reg;
    if (strlen(copy) != 2 || !starts_with_char(copy, 'r') || !isdigit(*(copy + 1))) {
        free(copyp);
        return INVALID;
    }

    reg = strtol((copy + 1), NULL, 0);
    if (reg >= 0 && reg <= 7) {
        result = reg;
    }

    free(copyp);
    return result;
}

/* We want to analyze the index expression (inside { })*/
char *get_index_expr(char *oper, int *error) {
    char *tmp = oper, *result, *index_expr;
    bool started = false;
    bool correct = false;
    int size = 0;

    while (*tmp != '\0') {
        if (!started) {
            if (*tmp == '{') {
                started = true;
                index_expr = tmp + 1;
            }
        } else {
            /* We don't want to pass smth like {{ax}*/
            if (*tmp == '{') {
                *error = INVALID;
                break;
            }

            size++;
            if (*tmp == '}') {
                correct = true;
                started = false;
                size--;
            }
        }

        /* another pair? It's too much for us. */
        if (correct && started) {
            *error = INVALID;
            break;
        }
        tmp++;
    }

    if (!started && !correct) {
        *error = NONE;
        return index_expr;
    }

    result = (char *) malloc((size + 1) * sizeof (char));
    strlcpy(result, index_expr, size + 1);
    return result;
}

/* Get the operation */
int get_opert_type(char *line) {
    char *tok;
    int result;
    line = remove_label(line);

    if (is_stop_opert(line)) {
        return STOP;
    }
    if (is_rts_opert(line)) {
        return RTS;
    }

    tok = malloc(4 * sizeof (char));
    strlcpy(tok, line, 4);


    /* Could be replaced with some preprocessor magic, I do it if I'll have time */
    if (strcmp(tok, "mov") == 0) {
        result = MOV;
    } else if (strcmp(tok, "cmp") == 0) {
        result = CMP;
    } else if (strcmp(tok, "add") == 0) {
        result = ADD;
    } else if (strcmp(tok, "sub") == 0) {
        result = SUB;
    } else if (strcmp(tok, "not") == 0) {
        result = NOT;
    } else if (strcmp(tok, "clr") == 0) {
        result = CLR;
    } else if (strcmp(tok, "lea") == 0) {
        result = LEA;
    } else if (strcmp(tok, "inc") == 0) {
        result = INC;
    } else if (strcmp(tok, "dec") == 0) {
        result = DEC;
    } else if (strcmp(tok, "jmp") == 0) {
        result = JMP;
    } else if (strcmp(tok, "bne") == 0) {
        result = BNE;
    } else if (strcmp(tok, "red") == 0) {
        result = RED;
    } else if (strcmp(tok, "prn") == 0) {
        result = PRN;
    } else if (strcmp(tok, "jsr") == 0) {
        result = JSR;

    } else {
        result = NONE;
    }
    free(tok);
    return result;
}

bool is_stop_opert(char *line) {
    bool result = false;
    char *tok = malloc(5 * sizeof (char));
    strlcpy(tok, line, 5);
    if (strcmp(tok, "stop") == 0) {
        result = true;
    }
    free(tok);
    return result;
}

bool is_rts_opert(char *line) {
    bool result = false;
    char *tok = malloc(4 * sizeof (char));
    strlcpy(tok, line, 4);
    if (strcmp(tok, "rts") == 0) {
        result = true;
    }
    free(tok);
    return result;
}

/*Damned strtok string modification, maybe I should've written my own tokenizer?*/
char *copy_line(char *line) {
    char *result;
    result = (char *) malloc(MAX_LINE_SIZE * sizeof (char));
    strcpy(result, line);
    return result;
}

/* Returns the label name, see tests for examples */
char *get_label_name(char* line) {
    char * first_tok, *garbage_line, *result;
    garbage_line = copy_line(line);

    /* Split by spaces */
    first_tok = strtok(garbage_line, " ");
    /* Do we have a colon, that indicates a label? */
    if (first_tok[strlen(first_tok) - 1] == ':') {
        /* Remove the colon */
        first_tok[strlen(first_tok) - 1] = '\0';
        result = (char *) malloc(strlen(first_tok) + 1);
        strcpy(result, first_tok);
        /* Don't want memory leaks in my house */
        free(garbage_line);
        return result;
    }
    free(garbage_line);
    return NULL;
}

/* Returns the symbol name for .extern and .entry instructions.
 * Do not use it in other cases. */
char *get_symbol_name(char *line) {
    line = remove_label(line);
    char *sec_tok, *garbage_line, *result;
    garbage_line = copy_line(line);
    strtok(garbage_line, " ");
    sec_tok = strtok(NULL, " ");
    if (sec_tok != NULL) {
        result = (char *) malloc(strlen(sec_tok) + 1);
        strcpy(result, sec_tok);
        free(garbage_line);
        return result;
    }
    free(garbage_line);
    return NULL;
}

/* Tear off the label after we used it*/
char *remove_label(char *line) {
    if (has_label(line)) {
        line = strchr(line, ':');
        line += 2;
    }
    return line;
}

/* Adds a symbol to the symbol table */
bool add_symbol(char *name, int adress, bool is_extrn, bool has_inst) {
    Symbol *symbol = (Symbol *) malloc(sizeof (Symbol));
    symbol->name = name;
    symbol->address = adress;
    symbol->is_extern = is_extrn;
    symbol->has_inst = has_inst;
    sglib_hashed_Symbol_add(symbol_table, symbol);
    return true;
}

/* Insert string data into data area */
void handle_string_instr(char *line) {
    char* data = get_string_data(line);
    int i = 0;
    if (data == NULL) {
        return;
    }

    char curr_char = 0;
    do {
        curr_char = *(data + (i++));
        data_area[dc++] = curr_char;
    } while (curr_char != 0);
    free(data);
}

/* Parses the string data from input line */
char *get_string_data(char *line) {
    line = remove_label(line);
    char *sec_tok, *result;
    Split *split;
    split = split_string(line, ' ');

    sec_tok = split -> tail;

    /* Check whether it's a correct string */
    if (starts_with_char(sec_tok, '\"') && ends_with_char(sec_tok, '\"')) {
        result = (char *) malloc(strlen(sec_tok));
        /* Copy without space */
        strcpy(result, (++sec_tok));
        free(split);
        /* Null terminate on last \" char */
        size_t length = strlen(sec_tok) - 1;
        result[length] = '\0';
        return result;
    }
    free(split);
    return NULL;
}

/* Insert numeric data into the data area*/
void handle_data_instr(char *line) {
    int tmp;
    Split *split;
    line = remove_label(line);
    split = split_string(line, ' ');
    char *tok = strtok(split -> tail, ",");

    while (tok != NULL) {
        tmp = strtol(tok, NULL, 0);
        data_area[dc++] = tmp;
        tok = strtok(NULL, ",");
    }
}

/* If something went wrong (invalid data) we need the ability to rollback to
 * previous state. */
void rollback_data(int original_dc) {
    for (; dc > original_dc; dc--) {
        data_area[dc] = 0;
    }
}

void handle_error(int error, char *line) {
    /*Do something with the error...*/
}

