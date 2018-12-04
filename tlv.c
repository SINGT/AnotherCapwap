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

	memset(box, 0, sizeof(*box));
	INIT_LIST_HEAD(&box->tlv_list);
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
	box->count++;

	return 0;
}

static uint8_t tlv_parse_u8(void *value)
{
	return *(uint8_t *)value;
}

static uint16_t tlv_parse_u16(void *value)
{
	uint16_t tmp;
	memcpy(&tmp, value, sizeof(uint16_t));
	return htons(tmp);
}

static uint32_t tlv_parse_u32(void *value)
{
	uint32_t tmp;
	memcpy(&tmp, value, sizeof(uint32_t));
	return htonl(tmp);
}

void tlv_box_set_how(struct tlv_box *box, uint32_t how)
{
	box->how = how;
}

/**
 * Parse a TLV format message to struct tlv_box
 */
int tlv_box_parse(struct tlv_box *box, void *buffer, int buffersize)
{
	struct tlv *tlv, *tmp;
	int offset = 0;
	int old_len = box->serialized_len;
	int i = 0;

	if (box->how & SERIAL_WITH_ID) {
		box->id = tlv_parse_u32(buffer);
		offset += sizeof(uint32_t);
	}
	while (offset < buffersize) {
		if (box->how & SERIAL_EACH_WITH_ID) {
			tlv->id = tlv_parse_u32(buffer + offset);
			offset += sizeof(uint32_t);
		}
		uint16_t type = tlv_parse_u16(buffer + offset);
		offset += sizeof(uint16_t);
		uint16_t length = tlv_parse_u16(buffer + offset);
		offset += sizeof(uint16_t);
		if (tlv_box_put_raw(box, type, length, buffer + offset))
			goto fail;
		offset += length;
		i++;
	}

	if (box->serialized_len - old_len != buffersize)
		goto fail;

	return 0;

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
	return -ENOMEM;
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
	free(box);

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

#define TLV_BOX_PUT_U16(buffer, offset, value)                                                     \
	do {                                                                                       \
		uint16_t __value = htons(value);                                                   \
		memcpy(buffer + offset, &__value, sizeof(uint16_t));                               \
		offset += sizeof(uint16_t);                                                        \
	} while (0)
#define TLV_BOX_PUT_U32(buffer, offset, value)                                                     \
	do {                                                                                       \
		uint32_t __value = htons(value);                                                   \
		memcpy(buffer + offset, &__value, sizeof(uint32_t));                               \
		offset += sizeof(uint32_t);                                                        \
	} while (0)

int tlv_box_serialize(struct tlv_box *box)
{
	int offset = 0;
	unsigned char *buffer;
	struct tlv *tlv;
	uint32_t id;
	uint16_t type;
	uint16_t length;

	if (!box) {
		return -EINVAL;
	}

	if (box->how &SERIAL_WITH_ID)
		box->serialized_len += sizeof(box->id);
	else if (box->how & SERIAL_EACH_WITH_ID)
		box->serialized_len += box->count * sizeof(tlv->id);

	if (!(buffer = (unsigned char *)malloc(box->serialized_len)))
		return -ENOMEM;

	if (box->how & SERIAL_WITH_ID) {
		TLV_BOX_PUT_U32(buffer, offset, box->id);
	}
	list_for_each_entry (tlv, &box->tlv_list, list) {
		if (box->how & SERIAL_EACH_WITH_ID) {
			TLV_BOX_PUT_U32(buffer, offset, tlv->id);
		}
		TLV_BOX_PUT_U16(buffer, offset, tlv->type);
		TLV_BOX_PUT_U16(buffer, offset, tlv->length);
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
