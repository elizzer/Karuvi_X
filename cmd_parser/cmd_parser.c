#include <stdio.h>
#include "cmd_parser.h"
#include "cli_log.h"

int8_t cmd_dispatch(const char *cmd, cmdEntry_t *cmd_table, int table_size)
{
    char key[32];
    char args[128];
    cmd_parse(cmd, key, 32, args, 128);

    int8_t cb_idx = cmd_tbl_search(key, cmd_table, table_size);
    if (cb_idx != -1)
    {
        cmd_table[cb_idx].func(NULL, args);
        return 0;
    }
    return -1;
}

int8_t cmd_tbl_search(const char *key, cmdEntry_t *cmd_table, int table_size)
{
    for (int i = 0; i < table_size; i++)
    {
        if (cmd_table[i].func != NULL && strcmp(cmd_table[i].key, key) == 0)
        {
            return i; // Success
        }
    }
    return -1; // Command not found
}

int8_t cmd_parse(const char *cmd, char *key, size_t key_size, char *args, size_t args_size)
{
    char *space = strchr(cmd, ' ');

    if (space)
    {
        size_t key_len = space - cmd;
        if (key_len >= key_size)
            key_len = key_size - 1;
        strncpy(key, cmd, key_len);
        key[key_len] = '\0';

        size_t args_len = strlen(space + 1);
        if (args_len >= args_size)
            args_len = args_size - 1;
        strncpy(args, space + 1, args_len);
        args[args_len] = '\0';
    }
    else
    {
        size_t key_len = strlen(cmd);
        if (key_len >= key_size)
            key_len = key_size - 1;
        strncpy(key, cmd, key_len);
        key[key_len] = '\0';
        args[0] = '\0';
    }

    return 0;
}

int8_t cmd_query_option(const char *cmd, const char *key, char *value)
{
    char buf[128];
    strncpy(buf, cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *key_p = strstr(buf, key);
    if (key_p == NULL) return -1;

    char *option = strtok(key_p, " ");
    char *option_key = strtok(option, "=");
    char *option_value = strtok(NULL, "=");

    printf("\n\r%s:%s", option_key, option_value ? option_value : "(null)");
    return 0;
}

bool cmd_query_flag(const char *cmd, const char *flag)
{
    if (flag == NULL) return false;
    if(strstr(flag,"--")==NULL){
        LOG_ERR("Invalid flag is given to search");
        return false;
    }
    char *f = strstr(cmd, flag);
    return (f != NULL);
}

// use gp --deamon