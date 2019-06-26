#include <zephyr.h>


#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/crypto.h>
#include <bsd/stdlib.h>
#include <host/conn_internal.h>
#include "hci_core.h"

#include <settings/settings.h>

#include <kernel.h>
#include <soc.h>

#include "spaceauth.h"

#define BLAKE2S_KEYBYTES    32

static uint16_t bas_svc_handle = 0;
static uint16_t bas_blvl_chr_handle = 0;
static uint16_t bas_blvl_chr_val_handle = 0;
static uint16_t auth_svc_handle = 0;
static uint16_t auth_challenge_chr_handle = 0;
static uint16_t auth_challenge_chr_value_handle = 0;
static uint16_t auth_response_chr_handle = 0;
static uint16_t auth_response_chr_value_handle = 0;
static uint16_t auth_response_chr_ccc_handle = 0;

uint8_t challenge[16];


static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params);

static u8_t notify_func(struct bt_conn *conn,
                        struct bt_gatt_subscribe_params *params,
                        const void *data, u16_t length){
    if (!data) {
		printk("[UNSUBSCRIBED]\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk("[NOTIFICATION] data %p length %u\n", data, length);

	return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_discover_params discover_params = {
        .uuid = &auth_service_uuid.uuid,
        .type = BT_GATT_DISCOVER_PRIMARY,
        .func = &discover_func,
        .start_handle = 0x0001,
        .end_handle = 0xffff,
};
static struct bt_gatt_subscribe_params subscribe_params = {
        .value = BT_GATT_CCC_INDICATE,
        .flags = 0,
        .notify = notify_func,
};

static u8_t discover_func(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params) {
    int err;

    if (!attr) {
        printf("Discover complete\n");
        (void) memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }
    printk("[ATTRIBUTE] handle %u\n", attr->handle);

    if (!bt_uuid_cmp(params->uuid, &auth_service_uuid.uuid)) {
        printf("found auth service handle\n");
        auth_svc_handle = attr->handle;
        //next up: search challenge chr
        discover_params.uuid = &auth_challenge_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            fprintf(stderr, "challenge chr discovery failed (err %d)\n", err);
        }
    } else if (!bt_uuid_cmp(params->uuid, &auth_challenge_uuid.uuid)) {
        printf("found auth challenge chr handle\n");
        auth_challenge_chr_handle = attr->handle;
        auth_challenge_chr_value_handle = attr->handle + 1;
        //next up: search response chr
        discover_params.start_handle = attr->handle + 2;
        discover_params.uuid = &auth_response_uuid.uuid;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            fprintf(stderr, "challenge chr discovery failed (err %d)\n", err);
        }
    } else if (!bt_uuid_cmp(params->uuid, &auth_response_uuid.uuid)) {
        printf("found auth response chr handle\n");
        auth_response_chr_handle = attr->handle;
        auth_response_chr_value_handle = attr->handle + 1;
        subscribe_params.value_handle = attr->handle + 1; // TODO

        //next up: search response chr cccd
        discover_params.start_handle = attr->handle + 2;
        discover_params.uuid = BT_UUID_GATT_CCC;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    } else if (!bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC)) {
        printf("found auth response chr cccd handle\n");
        auth_response_chr_ccc_handle = attr->handle;
        subscribe_params.ccc_handle = attr->handle;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            printk("Subscribe failed (err %d)\n", err);
        } else {
            printk("[SUBSCRIBED]\n");
        }

        //next up: search bas svc
        discover_params.uuid = BT_UUID_BAS;
        discover_params.start_handle = 0;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    } else if (!bt_uuid_cmp(params->uuid, BT_UUID_BAS)) {
        printf("found bas svc handle\n");
        bas_svc_handle = attr->handle;

        //next up: search bas blvl chr
        discover_params.start_handle = attr->handle + 1;
        discover_params.uuid = BT_UUID_BAS_BATTERY_LEVEL;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    } else if (!bt_uuid_cmp(params->uuid, BT_UUID_BAS_BATTERY_LEVEL)) {
        printf("found bas blvl chr handle\n");
        bas_blvl_chr_handle = attr->handle;
        bas_blvl_chr_val_handle = attr->handle + 1; // TODO

        return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_STOP;
}

static FILE *gl = NULL;

void initialize(void) {
    settings_subsys_init();
    FILE *c = fopen("central.txt", "r");
    struct bt_irk id_irk;
    bt_addr_le_t id;
    if (c && deserialize_id(c, &id, &id_irk)) {
        /*
        settings_save_one("bt/id", &id, sizeof(bt_addr_le_t));
        settings_save_one("bt/irk", &id_irk.val, 16);
        */
        int n = bt_id_create(&id, id_irk.val);
        if(n != 0){
            fprintf(stderr, "Oh no! The ID could not be set!");
        }
    } else {
        fprintf(stderr, "Couldn't load central ID!\n");
        exit(-1);
    }
    fclose(c);

    gl = open_guestlist("r");
    struct bt_keys keys;
    uint8_t spacekey[BLAKE2S_KEYBYTES];
    while (gl && deserialize_keys(gl, &keys, spacekey)) {
        space_add_id(&keys, spacekey);
        bt_keys_store(&keys);
    }
    bt_keys_foreach(BT_KEYS_ALL, space_save_id, stdout);
}

void finalize(void) {
    gl = freopen(NULL, "w+", gl);
    bt_keys_foreach(BT_KEYS_ALL, space_save_id, gl);
    fclose(gl);
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t type,
                         struct net_buf_simple *ad) {
    char addr_str[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    printk("Device found: %s (RSSI %d) (TYPE %u) (BONDED %u)\n", addr_str, rssi, type, bt_addr_le_is_bonded(BT_ID_DEFAULT, addr));

    /* We're only interested in directed connectable events from bonded devices*/
    if ((type != BT_LE_ADV_DIRECT_IND && type !=BT_LE_ADV_IND)|| !bt_addr_le_is_bonded(BT_ID_DEFAULT, addr)) {
        return;
    }

    printk("Connecting to device: %s (RSSI %d)\n", addr_str, rssi);

    if (bt_le_scan_stop()) {
        return;
    }

    bt_conn_create_le(addr, BT_LE_CONN_PARAM_DEFAULT);
}

static void connected(struct bt_conn *conn, u8_t err) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s (%u)\n", addr, err);
        bt_conn_unref(conn);
        int error = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
        if (error) {
        printk("Scanning failed to start (err %d)\n", error);
        }
        return;
    }

    printk("Connected: %s\n", addr);
    int error =bt_conn_security(conn, BT_SECURITY_FIPS);
    if (error) {
        printk("Failed to set security: %i\n", error);
        bt_conn_disconnect(conn, BT_HCI_ERR_INSUFFICIENT_SECURITY);
    }
    printk("Starting Discovery...\n");
    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Discover failed(err %d)\n", err);
        return;
    }
}

static void disconnected(struct bt_conn *conn, u8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected: %s (reason %u)\n", addr, reason);

    bt_conn_unref(conn);

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    }
}

static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
                              const bt_addr_le_t *identity) {
    char addr_identity[BT_ADDR_LE_STR_LEN];
    char addr_rpa[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
    bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

    printk("Identity resolved %s -> %s\n", addr_rpa, addr_identity);
}

static void security_changed(struct bt_conn *conn, bt_security_t level) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Security changed: %s level %u\n", addr, level);
}

static struct bt_conn_cb conn_callbacks = {
        .connected = connected,
        .disconnected = disconnected,
        .identity_resolved = identity_resolved,
        .security_changed = security_changed,
};


NATIVE_TASK(finalize, ON_EXIT, 0);

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

    err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);

    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning successfully started\n");
}

void main(int argc, char *argv[]) {
    initialize();

    int err;

    err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
    printk("Bluetooth initialized\n");

    bt_conn_cb_register(&conn_callbacks);
    bt_conn_auth_cb_register(NULL);
}
