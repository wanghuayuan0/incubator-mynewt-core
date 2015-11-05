/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <errno.h>
#include <assert.h>
#include "os/os.h"
#include "nimble/ble.h"
#include "nimble/hci_common.h"
#include "ble_hs_conn.h"
#include "ble_l2cap.h"

#define BLE_L2CAP_CHAN_MAX  32

static struct os_mempool ble_l2cap_chan_pool;

struct ble_l2cap_chan *
ble_l2cap_chan_alloc(void)
{
    struct ble_l2cap_chan *chan;

    chan = os_memblock_get(&ble_l2cap_chan_pool);
    if (chan == NULL) {
        return NULL;
    }

    memset(chan, 0, sizeof *chan);

    return chan;
}

void
ble_l2cap_chan_free(struct ble_l2cap_chan *chan)
{
    int rc;

    if (chan == NULL) {
        return;
    }

    rc = os_memblock_put(&ble_l2cap_chan_pool, chan);
    assert(rc == 0);
}

static int
ble_l2cap_parse_hdr(void *pkt, uint16_t len, struct ble_l2cap_hdr *l2cap_hdr)
{
    uint8_t *u8ptr;
    uint16_t off;

    if (len < BLE_L2CAP_HDR_SZ) {
        return EMSGSIZE;
    }

    off = 0;
    u8ptr = pkt;

    l2cap_hdr->blh_len = le16toh(u8ptr + off);
    off += 2;

    l2cap_hdr->blh_cid = le16toh(u8ptr + off);
    off += 2;

    if (len < BLE_L2CAP_HDR_SZ + l2cap_hdr->blh_len) {
        return EMSGSIZE;
    }

    return 0;
}

static struct ble_l2cap_chan *
ble_l2cap_chan_find(struct ble_hs_conn *conn, uint16_t cid)
{
    struct ble_l2cap_chan *chan;

    SLIST_FOREACH(chan, &conn->bhc_channels, blc_next) {
        if (chan->blc_cid == cid) {
            return chan;
        }
    }

    return NULL;
}

int
ble_l2cap_rx(struct ble_hs_conn *conn,
             struct hci_data_hdr *hci_hdr,
             void *pkt)
{
    struct ble_l2cap_chan *chan;
    struct ble_l2cap_hdr l2cap_hdr;
    uint8_t *u8ptr;
    int rc;

    u8ptr = pkt;

    rc = ble_l2cap_parse_hdr(u8ptr, hci_hdr->hdh_len, &l2cap_hdr);
    if (rc != 0) {
        return rc;
    }

    if (l2cap_hdr.blh_len != hci_hdr->hdh_len - BLE_L2CAP_HDR_SZ) {
        return EMSGSIZE;
    }

    chan = ble_l2cap_chan_find(conn, l2cap_hdr.blh_cid);
    if (chan == NULL) {
        return ENOENT;
    }

    /* XXX: Append incoming data to channel buffer. */

    rc = chan->blc_rx_fn(conn, chan);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ble_l2cap_init(void)
{
    os_membuf_t *chan_mem;
    int rc;

    chan_mem = malloc(
        OS_MEMPOOL_SIZE(BLE_L2CAP_CHAN_MAX,
                        sizeof (struct ble_l2cap_chan)));
    if (chan_mem == NULL) {
        return ENOMEM;
    }

    rc = os_mempool_init(&ble_l2cap_chan_pool, BLE_L2CAP_CHAN_MAX,
                         sizeof (struct ble_l2cap_chan),
                         chan_mem, "ble_l2cap_chan_pool");
    if (rc != 0) {
        return EINVAL; // XXX
    }

    return 0;
}