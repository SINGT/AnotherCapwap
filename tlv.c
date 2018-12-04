#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "tlv.h"

void tlv_box_init(struct tlv_box *box)
{
	if (!box)
		return;

	INIT_LIST_HEAD(&box->tlv_list);
	box->serialized_buffer = NULL;
	box->serialized_len = 0;
}

struct tlv_box *tlv_box_create()
{
	struct tlv_box *box = (struct tlv_box *)malloc(sizeof(struct tlv_box));
	if (!box)
		return NULL;

	memset(box, 0, sizeof(*box));
	INIT_LIST_HEAD(&box->tlv_list);

	return box;
}

int tlv_box_put_raw(struct tlv_box *box, uint16_t type, uint16_t length, const void *value)
{
	struct tlv *tlv;

	if (!box || !value)
		return -EINVAL;

	tlv = (struct tlv *)calloc(1, sizeof(struct tlv));
	if (!tlv)
		return -ENOMEM;
	tlv->type = type;
	tlv->length = length;
	if (!(tlv->value = malloc(length)))
		return -ENOMEM;
	memcpy(tlv->value, value, length);
	list_add_tail(&tlv->list, &box->tlv_list);
	box->serialized_len += sizeof(tlv->type) + sizeof(tlv->length) + length;

	return 0;
}

/**
 * Parse a TLV format message to struct tlv_box
 */
struct tlv_box *tlv_box_parse(struct tlv_box *_box, void *buffer, int buffersize)
{
	struct tlv_box *box = _box ? _box : tlv_box_create();
	struct tlv *tlv, *tmp;
	int offset = 0;
	int old_len = box->serialized_len;
	int i = 0;

	while (offset < buffersize) {
		uint16_t type = (*(uint16_t *)(buffer + offset));
		offset += sizeof(uint16_t);
		uint16_t length = (*(uint16_t *)(buffer + offset));
		offset += sizeof(uint16_t);
		if (tlv_box_put_raw(box, ntohs(type), ntohs(length), buffer + offset))
			goto fail;
		offset += ntohs(length);
		i++;
	}

	if (box->serialized_len - old_len != buffersize)
		goto fail;

	return box;

fail:
	list_for_each_entry_safe_reverse(tlv, tmp, &box->tlv_list, list) {
		if (i == 0)
			break;
		if (tlv->value)
			free(tlv->value);
		list_del(&tlv->list);
		free(tlv);
		i--;
	}
	if (!_box)
		free(box);
	return NULL;
}

int inline tlv_box_put_string(struct tlv_box *box, uint16_t type, char *value)
{
	return tlv_box_put_raw(box, type, strlen(value), value);
}

int tlv_box_put_box(struct tlv_box *box, uint16_t type, struct tlv_box *object)
{
	int err;
	if ((err = tlv_box_serialize(object)))
		return err;
	return tlv_box_put_raw(box, type, tlv_box_get_size(object), tlv_box_get_buffer(object));
}

void tlv_box_destroy(struct tlv_box *box)
{
	struct tlv *tlv, *tmp;
	list_for_each_entry_safe (tlv, tmp, &box->tlv_list, list) {
		if (tlv->value)
			free(tlv->value);
		list_del(&tlv->list);
		free(tlv);
	}

	if (box->serialized_buffer) {
		free(box->serialized_buffer);
		box->serialized_buffer = NULL;
	}

	return;
}

void *tlv_box_get_buffer(struct tlv_box *box)
{
	return box->serialized_buffer;
}

int tlv_box_get_size(struct tlv_box *box)
{
	return box->serialized_len;
}

int tlv_box_serialize(struct tlv_box *box)
{
	int offset = 0;
	unsigned char *buffer;
	struct tlv *tlv;
	uint16_t type;
	uint16_t length;

	if (!box) {
		return -EINVAL;
	}

	if (!(buffer = (unsigned char *)malloc(box->serialized_len)))
		return -ENOMEM;

	list_for_each_entry (tlv, &box->tlv_list, list) {
		type = htons(tlv->type);
		memcpy(buffer + offset, &type, sizeof(tlv->type));
		offset += sizeof(tlv->type);
		length = htons(tlv->length);
		memcpy(buffer + offset, &length, sizeof(tlv->length));
		offset += sizeof(tlv->length);
		memcpy(buffer + offset, tlv->value, tlv->length);
		offset += tlv->length;
	}

	box->serialized_buffer = buffer;

	return 0;
}

struct tlv *tlv_box_find_type(struct tlv_box *box, uint16_t type)
{
	struct tlv *tlv;

	list_for_each_entry(tlv, &box->tlv_list, list) {
		if (tlv->type == type)
			return tlv;
	}
	return NULL;
}

int tlv_box_get_object(struct tlv_box *box, uint16_t type, struct tlv_box **object)
{
	struct tlv *tlv;
	if (!box || !object)
		return -EINVAL;

	tlv = tlv_box_find_type(box, type);
	if (!tlv)
		*object = NULL;
	else
		*object = tlv_box_parse(NULL, tlv->value, tlv->length);

	return 0;
}

void tlv_box_print(struct tlv_box *box)
{
	struct tlv *tlv;
	uint16_t i;

	list_for_each_entry (tlv, &box->tlv_list, list) {
		printf("{type %d, len %d, value: ", tlv->type, tlv->length);
		for (i = 0; i < tlv->length; i++) {
			printf("0x%02hhx ", *(uint8_t *)(tlv->value + i));
		}
		printf("} -> ");
	}
	printf("end\n");
}
