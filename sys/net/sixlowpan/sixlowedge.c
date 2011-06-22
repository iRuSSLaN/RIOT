#include <stdint.h>
#include <stdlib.h>
#include "ieee802154_frame.h"
#include "sixlowedge.h"
#include "sixlowip.h"
#include "serialnumber.h"
#include "sixlowerror.h"

abr_cache_t *abr_info;
uint16_t abro_version;

uint16_t get_next_abro_version();
void init_edge_router_info(ipv6_addr_t *abr_addr);
uint8_t abr_info_add_context(lowpan_context_t *context);
uint8_t abr_info_add_prefix(plist_t *prefix);

uint16_t get_next_abro_version() {
    abro_version = serial_add16(abro_version, 1);
    return abro_version;
}

uint8_t edge_initialize(transceiver_type_t trans,ipv6_addr_t *edge_router_addr) {
    /* only allow addresses generated accoding to 
     * RFC 4944 (Section 6) & RFC 2464 (Section 4) from short address 
     * -- for now
     */
    if (    edge_router_addr->uint16[4] != HTONS(IEEE_802154_PAN_ID ^ 0x0200) ||
            edge_router_addr->uint16[5] != HTONS(0x00FF) ||
            edge_router_addr->uint16[6] != HTONS(0xFE00)
        ) {
        return SIXLOWERROR_ADDRESS;
    }
    
    // radio-address is 8-bit so this must be tested extra
    if (edge_router_addr->uint8[14] != 0) {
        return SIXLOWERROR_ADDRESS;
    }
    
    sixlowpan_init(trans,edge_router_addr->uint8[15]);
    
    init_edge_router_info(edge_router_addr);
    
    ipv6_init_iface_as_router();
    
    return SUCCESS;
}

lowpan_context_t *edge_define_context(uint8_t cid, ipv6_addr_t *prefix, uint8_t prefix_len, uint16_t lifetime) {
    lowpan_context_t *context;
    
    context = lowpan_context_update(cid, prefix, prefix_len, OPT_6CO_FLAG_C_VALUE_SET, lifetime);
    abr_info_add_context(context);
    return context;
}

lowpan_context_t *edge_alloc_context(ipv6_addr_t *prefix, uint8_t prefix_len, uint16_t lifetime) {
    lowpan_context_t *context = lowpan_context_lookup(prefix);
    
    if (context != NULL && context->length == prefix_len) {
        context = edge_define_context(context->num, prefix, prefix_len, lifetime);
    }
    
    context = NULL;
    for (int i = 0; i < LOWPAN_CONTEXT_MAX; i++) {
        if (lowpan_context_num_lookup(i) != NULL) {
            context = edge_define_context(i, prefix, prefix_len, lifetime);
        }
    }
    return context;
}

uint8_t abr_info_add_context(lowpan_context_t *context) {
    if (context == NULL) return SIXLOWERROR_NULLPTR;
    uint16_t abro_version = get_next_abro_version();
    int i;
    for (i = 0; i < abr_info->contexts_num; i++) {
        if (abr_info->contexts[i]->num == context->num) {
            abr_info->contexts[i] = context;
            abr_info->version = abro_version;
            return SUCCESS;
        }
    }
    
    if (abr_info->contexts_num == LOWPAN_CONTEXT_MAX) {
        return SIXLOWERROR_ARRAYFULL;
    }
    
    abr_info->contexts[abr_info->contexts_num++] = context;
    abr_info->version = abro_version;
    return SUCCESS;
}

uint8_t abr_info_add_prefix(plist_t *prefix) {
    if (prefix == NULL) return SIXLOWERROR_NULLPTR;
    if (abr_info->prefixes_num == OPT_PI_LIST_LEN) {
        return SIXLOWERROR_ARRAYFULL;
    }
    
    uint16_t abro_version = get_next_abro_version();
    
    abr_info->prefixes[abr_info->prefixes_num++] = prefix;
    abr_info->version = abro_version;
    
    return SUCCESS;
}

void init_edge_router_info(ipv6_addr_t *abr_addr) {
    uint16_t abro_version = get_next_abro_version();
    ipv6_addr_t prefix;
    plist_t *prefix_info;
    lowpan_context_t *context;
    
    if (abr_info == NULL)
        abr_info = abr_update_cache(abro_version,abr_addr,NULL,0,NULL,0);
    else
        abr_info = abr_update_cache(
                abro_version,
                abr_addr,
                abr_info->contexts,
                abr_info->contexts_num,
                abr_info->prefixes,
                abr_info->prefixes_num
            );
    
    ipv6_iface_add_addr(abr_addr,ADDR_STATE_PREFERRED,0,0,ADDR_TYPE_UNICAST);
    ipv6_set_prefix(&prefix, abr_addr);
    prefix_info = plist_add(&prefix, 64, OPT_PI_VLIFETIME_INFINITE,0,1,OPT_PI_FLAG_A);
    abr_info_add_prefix(prefix_info);
    
    context = edge_define_context(0, &prefix, 64, 5);  // has to be reset some time later
    abr_info_add_context(context);
}

abr_cache_t *get_edge_router_info() {
    return abr_info;
}
