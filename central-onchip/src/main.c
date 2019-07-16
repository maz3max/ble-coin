#include <zephyr.h>
#include <sys/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <logging/log.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app);

static int cmd_coin_add(const struct shell *shell, size_t argc, char **argv){
	LOG_INF("ADDING COIN");
	LOG_INF("argc = %d", argc);
}

static int cmd_coin_del(const struct shell *shell, size_t argc, char **argv){
	LOG_INF("DELETING COIN");
	LOG_INF("argc = %d", argc);
}

/* Creating subcommands (level 1 command) array for command "coin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_coin,
        SHELL_CMD(add, NULL, "Add coin to the whitelist.", cmd_coin_add),
        SHELL_CMD(del, NULL, "Delete coin from to whitelist.", cmd_coin_del),
        SHELL_SUBCMD_SET_END
);
/* Creating root (level 0) command "coin" */
SHELL_CMD_REGISTER(coin, &sub_coin, "Demo commands", NULL);

void main(void)
{

}
