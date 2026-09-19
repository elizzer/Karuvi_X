#ifndef CMD_PARSER_H
#define CMD_PARSER_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

typedef void (*command_handler_t)(char *args);
typedef void (*cmd_fp_t)(void *, char *);
typedef struct
{
    char key[32];
    cmd_fp_t func;
    const char *help_str;
} cmdEntry_t;

int8_t cmd_dispatch(const char *cmd, cmdEntry_t *table, int table_size);
int8_t cmd_parse(const char *cmd, char *key, size_t key_size, char *args, size_t args_size);
int8_t cmd_tbl_search(const char *key, cmdEntry_t *cmd_table, int table_size);
int8_t cmd_query_option(const char *cmd, const char *key, char *value);
bool cmd_query_flag(const char *cmd, const char *flag);

#endif /* CMD_PARSER_H */