#ifndef GUESTLIST_H
#define GUESTLIST_H

#include <keys.h>
#include <bluetooth/hci.h>
#include <stdio.h>
#include <regex.h>
#include <sys/file.h>


bool deserialize_id(FILE *fp,
                            bt_addr_le_t *id, struct bt_irk *id_irk) {
    const char *rx_str = "^(([[:xdigit:]]{2}\\:){5}[[:xdigit:]]{2})\\s*"    // addr (1,2)
                         "(random|public)\\s*"                              // type of addr (3)
                         "([[:xdigit:]]{32})\\s*";                          // irk (4)
    regex_t rx;
    regmatch_t matches[5];
    regcomp(&rx, rx_str, REG_EXTENDED);
    char *line = NULL;
    size_t len = 0;
    if (getline(&line, &len, fp) > 0 && regexec(&rx, line, 5, matches, 0) == 0 && matches[0].rm_so != -1) {
        for (int i = 0; i < 6; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 3)], line[matches[1].rm_so + (i * 3) + 1], 0};
            id->a.val[5 - i] = strtol(buf, NULL, 16);
        }
        if (!strncmp("random", line + matches[3].rm_so, 6)) {
            id->type = BT_ADDR_LE_RANDOM;
        } else if (!strncmp("public", line + matches[3].rm_so, 6)) {
            id->type = BT_ADDR_LE_PUBLIC;
        }
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[4].rm_so + (i * 2)], line[matches[4].rm_so + (i * 2) + 1], 0};
            id_irk->val[i] = strtol(buf, NULL, 16);
        }
    } else {
        return false;
    }
    regfree(&rx);
    return true;
}

bool deserialize_keys(FILE *fp,
                              struct bt_keys *keys, uint8_t *spacekey) {
    const char *rx_str = "^(([[:xdigit:]]{2}\\:){5}[[:xdigit:]]{2})\\s*"    // addr (1,2)
                         "(random|public)\\s*"                              // type of addr (3)
                         "([[:xdigit:]]{32})\\s*"                           // irk (4)
                         "([[:xdigit:]]{32})\\s*"                           // ltk (5)
                         "([[:xdigit:]]{32})\\s*" "([[:digit:]]{1,10})\\s*" // local-csrk with cnt (6,7)
                         "([[:xdigit:]]{32})\\s*" "([[:digit:]]{1,10})\\s*" // remote-csrk with cnt (8,9)
                         "([[:xdigit:]]{64})";                              // space-key (10)
    regex_t rx;
    regmatch_t matches[11];
    regcomp(&rx, rx_str, REG_EXTENDED);
    char *line = NULL;
    size_t len = 0;
    if (getline(&line, &len, fp) > 0 && regexec(&rx, line, 11, matches, 0) == 0 && matches[0].rm_so != -1) {
        keys->enc_size = BT_ENC_KEY_SIZE_MAX;
        keys->flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC);
        keys->keys = (BT_KEYS_IRK | BT_KEYS_LOCAL_CSRK | BT_KEYS_REMOTE_CSRK | BT_KEYS_LTK_P256);
        keys->id = BT_ID_DEFAULT;

        for (int i = 0; i < 6; ++i) {
            char buf[3] = {line[matches[1].rm_so + (i * 3)], line[matches[1].rm_so + (i * 3) + 1], 0};
            keys->addr.a.val[5 - i] = strtol(buf, NULL, 16);
        }
        if (!strncmp("random", line + matches[3].rm_so, 6)) {
            keys->addr.type = BT_ADDR_LE_RANDOM;
        } else if (!strncmp("public", line + matches[3].rm_so, 6)) {
            keys->addr.type = BT_ADDR_LE_PUBLIC;
        }
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[4].rm_so + (i * 2)], line[matches[4].rm_so + (i * 2) + 1], 0};
            keys->irk.val[i] = strtol(buf, NULL, 16);
        }
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[5].rm_so + (i * 2)], line[matches[5].rm_so + (i * 2) + 1], 0};
            keys->ltk.val[i] = strtol(buf, NULL, 16);
        }
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[6].rm_so + (i * 2)], line[matches[6].rm_so + (i * 2) + 1], 0};
            keys->local_csrk.val[i] = strtol(buf, NULL, 16);
        }
        for (int i = 0; i < 16; ++i) {
            char buf[3] = {line[matches[8].rm_so + (i * 2)], line[matches[8].rm_so + (i * 2) + 1], 0};
            keys->remote_csrk.val[i] = strtol(buf, NULL, 16);
        }
        {
            char buf[11];
            memset(buf,0,11);
            strncpy(buf, line + matches[7].rm_so, matches[7].rm_eo - matches[7].rm_so);
            keys->local_csrk.cnt = strtol(buf, NULL, 10);
            memset(buf,0,11);
            strncpy(buf, line + matches[9].rm_so, matches[9].rm_eo - matches[9].rm_so);
            keys->remote_csrk.cnt = strtol(buf, NULL, 10);
        }
        for (int i = 0; i < 32; ++i) {
            char buf[3] = {line[matches[10].rm_so + (i * 2)], line[matches[10].rm_so + (i * 2) + 1], 0};
            spacekey[i] = strtol(buf, NULL, 16);
        }
    } else {
        return false;
    }
    regfree(&rx);
    return true;
}

void serialize_id(bt_addr_le_t *id, struct bt_irk *id_irk,
                          FILE *fp) {
    // addr and addr type
    const char *type = (id->type == BT_ADDR_LE_PUBLIC) ? "public" : "random";
    fprintf(fp, "%02X:%02X:%02X:%02X:%02X:%02X %s ",
            id->a.val[5], id->a.val[4], id->a.val[3],
            id->a.val[2], id->a.val[1], id->a.val[0], type);
    // irk
    for (size_t i = 0; i < 16; ++i) {
        fprintf(fp, "%02X", id_irk->val[i]);
    }
    fprintf(fp, "\n");
}

void serialize_keys(struct bt_keys *keys, uint8_t *spacekey,
                            FILE *fp) {
    // addr and addr type
    const char *type = (keys->addr.type == BT_ADDR_LE_PUBLIC) ? "public" : "random";
    fprintf(fp, "%02X:%02X:%02X:%02X:%02X:%02X %s ",
            keys->addr.a.val[5], keys->addr.a.val[4], keys->addr.a.val[3],
            keys->addr.a.val[2], keys->addr.a.val[1], keys->addr.a.val[0], type);
    // irk
    for (size_t i = 0; i < 16; ++i) {
        fprintf(fp, "%02X", keys->irk.val[i]);
    }
    fprintf(fp, " ");
    // ltk
    for (size_t i = 0; i < 16; ++i) {
        fprintf(fp, "%02X", keys->ltk.val[i]);
    }
    fprintf(fp, " ");
    // local csrk
    for (size_t i = 0; i < 16; ++i) {
        fprintf(fp, "%02X", keys->local_csrk.val[i]);
    }
    fprintf(fp, " %u ", keys->local_csrk.cnt);
    // remote csrk
    for (size_t i = 0; i < 16; ++i) {
        fprintf(fp, "%02X", keys->remote_csrk.val[i]);
    }
    fprintf(fp, " %u ", keys->remote_csrk.cnt);
    // space-key
    for (size_t i = 0; i < 32; ++i) {
        fprintf(fp, "%02X", spacekey[i]);
    }
    fprintf(fp, "\n");
}

FILE *open_guestlist(const char* mode) {
    FILE *fp = fopen("coins.txt", mode);
    if (fp && flock(fileno(fp), LOCK_EX | LOCK_NB) == 0) {
        return fp;
    }
    fprintf(stderr, "Failed to open guest list\n");
    exit(-1);
    return NULL;
}
#endif