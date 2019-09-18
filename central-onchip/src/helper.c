#include "helper.h"
// zephyr includes
#include <logging/log.h>
#include <shell/shell.h>
#include <power/reboot.h>
#include <settings/settings.h>
#include <storage/flash_map.h>
#include <keys.h> //use of internal keys API for bt_keys etc.
#include <settings.h> //use of internal bt settings API
// stdlib includes
#include <stdlib.h>
#include <ctype.h>
// own includes
#include "spaceauth.h"

LOG_MODULE_REGISTER(helper);

int parse_addr(const char *addr, bt_addr_le_t *result) {
    if (strlen(addr) != 17) {
        LOG_ERR("wrong address length");
        return -EINVAL;
    } else {
        for (size_t i = 2; i < 15; i += 3) {
            if (addr[i] != ':') {
                LOG_ERR("invalid address");
                return -EINVAL;
            }
        }
        for (size_t i = 0; i < 6; ++i) {
            size_t off = 3 * i;
            char buf[3] = {addr[off], addr[off + 1], 0};
            if (isxdigit(buf[0]) && isxdigit(buf[1])) {
                result->a.val[5 - i] = strtol(buf, NULL, 16);
            } else {
                LOG_ERR("invalid address");
                return -EINVAL;
            }
        }
    }
    result->type = BT_ADDR_LE_RANDOM;

    return 0;
}

int parse_hex(const char *str, size_t n, uint8_t *out) {
    if (strlen(str) != 2 * n) {
        LOG_ERR("wrong hex string length");
        return -EINVAL;
    }
    for (size_t i = 0; i < n; ++i) {
        char buf[] = {str[2 * i], str[2 * i + 1], 0};
        if (isxdigit(buf[0]) && isxdigit(buf[1])) {
            out[i] = strtol(buf, NULL, 16);
        } else {
            LOG_ERR("invalid hex string");
            return -EINVAL;
        }
    }
    LOG_HEXDUMP_DBG(out, n, "parsed data");
    return 0;
}

// static settings commands

/**
 * command to load settings from storage
 */
static int cmd_settings_load(const struct shell *shell, size_t argc, char **argv) {
    settings_load();
    shell_info(shell, "done");
    return 0;
}

/**
 * command to clear storage
 */
static int cmd_settings_clear(const struct shell *shell, size_t argc, char **argv) {
    const struct flash_area *fap;
    flash_area_open(DT_FLASH_AREA_STORAGE_ID, &fap);
    int rc = flash_area_erase(fap, 0, fap->fa_size);
    if (rc != 0) {
        shell_error(shell, "cannot get flash area");
    }
    shell_info(shell, "Storage cleared, please reboot. No, loading does not suffice.");
    return 0;
}

/**
 * command to performs hard-reset of chip
 */
static int cmd_reboot(const struct shell *shell, size_t argc, char **argv) {
    sys_reboot(SYS_REBOOT_COLD);
    return 0; // just to make compiler happy
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_settings,
                               SHELL_CMD(load, NULL, "load all settings from storage", cmd_settings_load),
                               SHELL_CMD(clear, NULL, "clear storage (needs reboot)", cmd_settings_clear),
                               SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(settings, &sub_settings, "commands to manage settings", NULL);

SHELL_CMD_REGISTER(reboot, NULL, "perform cold system reboot", cmd_reboot);

// static central and coin management commands

/**
 * command to add coins by specifying the address and the keys
 * can fail on invalid input or full buffer
 */
static int cmd_coin_add(const struct shell *shell, size_t argc, char **argv) {
    struct bt_keys keys = {
            .keys = (BT_KEYS_IRK | BT_KEYS_LTK_P256),
            .flags = (BT_KEYS_AUTHENTICATED | BT_KEYS_SC),
            .id = BT_ID_DEFAULT,
            .enc_size = BT_ENC_KEY_SIZE_MAX,
    };
    uint8_t spacekey[32] = {0};

    if (argc != 5) {
        shell_error(shell, "incorrect number of arguments");
        return EINVAL;
    }

    // address
    int ret = parse_addr(argv[1], &keys.addr);
    if (ret) {
        shell_error(shell, "invalid address");
        return ret;
    }
    LOG_DBG("valid address");
    // identity resolving key
    ret = parse_hex(argv[2], 16, keys.irk.val);
    if (ret) {
        shell_error(shell, "invalid IRK");
        return ret;
    }
    LOG_DBG("valid IRK");
    // long-term key
    ret = parse_hex(argv[3], 16, keys.ltk.val);
    if (ret) {
        shell_error(shell, "invalid LTK");
        return ret;
    }
    LOG_DBG("valid LTK");
    // space key
    ret = parse_hex(argv[4], 32, spacekey);
    if (ret) {
        shell_error(shell, "invalid spacekey");
        return ret;
    }
    LOG_DBG("valid space key");

    ret = spacekey_add(&keys.addr, spacekey);
    if (ret) {
        shell_error(shell, "spacekey_add failed with %i", ret);
        return ret;
    }
    char key[BT_SETTINGS_KEY_MAX];
    bt_settings_encode_key(key, sizeof(key), "keys", &keys.addr, NULL);
    bt_keys_store(&keys);
    settings_set_value(key, keys.storage_start, BT_KEYS_STORAGE_LEN);
    shell_info(shell, "done");
    return 0;
}

/**
 * command to delete a coin by specifying its address
 */
static int cmd_coin_del(const struct shell *shell, size_t argc, char **argv) {
    bt_addr_le_t addr;
    if (argc != 2) {
        shell_error(shell, "incorrect number of arguments");
        return EINVAL;
    }
    int ret = parse_addr(argv[1], &addr);
    if (ret) {
        shell_error(shell, "invalid address");
        return ret;
    }
    LOG_DBG("valid address");

    ret = bt_unpair(BT_ID_DEFAULT, &addr);
    if (ret) {
        shell_error(shell, "could not unpair this address (err %d)", ret);
        //LEAVE OUT 'return ret;' DELIBERATELY
    }
    spacekey_del(&addr);
    shell_info(shell, "done");
    return 0;
}
/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_coin,
                               SHELL_CMD(add, NULL, "usage: coin add <addr> <irk> <ltk> <spacekey>",
                                         cmd_coin_add),
                               SHELL_CMD(del, NULL, "usage: coin del <addr> ", cmd_coin_del),
                               SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(coin, &sub_coin, "commands to manage coins", NULL);

/**
 * command to print all registered spacekeys
 */
static int cmd_print_spacekeys(const struct shell *shell, size_t argc, char **argv) {
    spacekeys_print(shell);
    shell_info(shell, "done");
    return 0;
}

// helper function for cmd_print_bonds
static void print_bond_func(struct bt_keys *keys, void *data) {
    const struct shell *shell = data;
    shell_print(shell, "[%02X:%02X:%02X:%02X:%02X:%02X] keys: %u, flags: %u", keys->addr.a.val[5], keys->addr.a.val[4],
                keys->addr.a.val[3], keys->addr.a.val[2], keys->addr.a.val[1], keys->addr.a.val[0], keys->keys,
                keys->flags);
}

/**
 * command to print all registered BLE bonds
 */
static int cmd_print_bonds(const struct shell *shell, size_t argc, char **argv) {
    bt_keys_foreach(BT_KEYS_ALL, print_bond_func, (void *) shell);
    shell_info(shell, "done");
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_stats,
                               SHELL_CMD(spacekey, NULL, "prints space keys", cmd_print_spacekeys),
                               SHELL_CMD(bonds, NULL, "prints bonds", cmd_print_bonds),
                               SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(stats, &sub_stats, "commands to print internal state", NULL);

/**
 * command to set addr and IRK of central
 */
static int cmd_central_setup(const struct shell *shell, size_t argc, char **argv) {
    bt_addr_le_t addr;
    int ret = parse_addr(argv[1], &addr);
    if (ret) {
        shell_error(shell, "invalid address");
        return ret;
    }
    LOG_DBG("valid address");
    uint8_t irk[16];
    ret = parse_hex(argv[2], 16, irk);
    if (ret) {
        shell_error(shell, "invalid IRK");
        return ret;
    }
    LOG_DBG("valid IRK");
    settings_save_one("bt/irk", irk, sizeof(irk));
    settings_save_one("bt/id", &addr, sizeof(bt_addr_le_t));
    settings_set_value("bt/irk", irk, sizeof(irk));
    settings_set_value("bt/id", &addr, sizeof(bt_addr_le_t));
    shell_info(shell, "done");
    return 0;
}

SHELL_CMD_REGISTER(central_setup, NULL, "usage: central_setup <addr> <irk>", cmd_central_setup);