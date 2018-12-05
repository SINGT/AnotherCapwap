/*
 *  COPYRIGHT NOTICE
 *  Copyright (C) 2015, Jhuster, All Rights Reserved
 *  Author: Jhuster(lujun.hust@gmail.com)
 *
 *  https://github.com/Jhuster/TLV
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2.1 of the License,
 *  or (at your option) any later version.
 */
#ifndef _TLV_H_
#define _TLV_H_

#include <stdint.h>
#include "list.h"
#include "capwap_common.h"

struct tlv {
	uint32_t id;
	struct list_head list;
	uint16_t type;
	uint16_t length;
	void *value;
	int flag;
};

#define SERIAL_WITH_ID		BIT(0)
#define SERIAL_EACH_WITH_ID	BIT(1)
struct tlv_box {
	uint32_t id;
	uint32_t count;
	uint32_t how;
	struct list_head tlv_list;
	void *serialized_buffer;
	int serialized_len;
};

#define tlv_box_for_each_tlv(box, tlv, _type, _len, _value)                                        \
	for (tlv = list_entry((&(box)->tlv_list)->next, typeof(*tlv), list); _len = tlv->length,   \
	    _value = tlv->value, _type = tlv->type, &tlv->list != (&(box)->tlv_list);              \
	     tlv = list_entry(tlv->list.next, typeof(*tlv), list))

void tlv_box_init(struct tlv_box *box);
struct tlv_box *tlv_box_create();
void tlv_box_destroy(struct tlv_box *box);

void *tlv_box_get_buffer(struct tlv_box *box);
int tlv_box_get_size(struct tlv_box *box);

void tlv_box_set_how(struct tlv_box *box, uint32_t how);

/**
 * tlv flags:
 * TLV_NOCPY: don't copy message buffer, use the pointer directly.
 */
#define TLV_NOCPY	BIT(0)
#define TLV_NOFREE	BIT(1)
int tlv_box_put_raw(struct tlv_box *box, uint16_t type, struct message *msg, int flag);
int tlv_box_put_string(struct tlv_box *box, uint16_t type, char *value, int flag);
int tlv_box_put_box(struct tlv_box *box, uint16_t type, struct tlv_box *object, int flag);

int tlv_box_parse(struct tlv_box *_box, void *buffer, int buffersize);
int tlv_box_serialize(struct tlv_box *box);

struct tlv *tlv_box_find_type(struct tlv_box *box, uint16_t type);

#endif //_TLV_H_
