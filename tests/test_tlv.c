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
#include <stdio.h>
#include <string.h>
#include "tlv.h"

#define TEST_TYPE_0 0x00
#define TEST_TYPE_1 0x01
#define TEST_TYPE_2 0x02
#define TEST_TYPE_3 0x03
#define TEST_TYPE_4 0x04
#define TEST_TYPE_5 0x05
#define TEST_TYPE_6 0x06
#define TEST_TYPE_7 0x07
#define TEST_TYPE_8 0x08
#define TEST_TYPE_9 0x09

#define LOG(format,...) printf(format, ##__VA_ARGS__)

int main(int argc, char const *argv[])
{
    struct tlv_box *box = tlv_box_create();
    tlv_box_put_string(box, TEST_TYPE_7, (char *)"hello world !");
    unsigned char array[6] = {1, 2, 3, 4, 5, 6};
    tlv_box_put_raw(box,TEST_TYPE_8, 6, array);

    if (tlv_box_serialize(box) != 0) {
        LOG("box serialize failed !\n");
        return -1;
    }

    LOG("box serialize success, %d bytes \n", tlv_box_get_size(box));

    struct tlv_box *boxes = tlv_box_create();
    tlv_box_put_box(boxes, TEST_TYPE_9, box);

    if (tlv_box_serialize(boxes) != 0) {
        LOG("boxes serialize failed !\n");
        return -1;
    }

    LOG("boxes serialize success, %d bytes \n", tlv_box_get_size(boxes));

    struct tlv_box *parsedBoxes = tlv_box_parse(NULL, tlv_box_get_buffer(boxes), tlv_box_get_size(boxes));

    LOG("boxes parse success, %d bytes \n", tlv_box_get_size(parsedBoxes));

    struct tlv *tlv;
    uint16_t type, length;
    void *value;
	struct tlv_box *parsedBox;
    tlv_box_for_each_tlv(parsedBoxes, tlv, type, length, value) {
	    switch (tlv->type) {
	    case TEST_TYPE_9:
		    if (!(parsedBox = tlv_box_parse(NULL, tlv->value, tlv->length))) {
			    LOG("tlv box parsed from boxes failed\n");
			    return -1;
		    } else {
			    LOG("box parse success, %d bytes \n", tlv_box_get_size(parsedBox));

            }
            break;
        default:
            LOG("shouldn't be here");
            break;
	    }
    }
    tlv_box_for_each_tlv(parsedBox, tlv, type, length, value) {
	    switch (type) {
	    case TEST_TYPE_1:
		    LOG("tlv_box_get_char success %d:%c \n", length, *(uint8_t *)value);
		    break;
	    case TEST_TYPE_2:
		    LOG("tlv_box_get_short success %d:%d \n", length, *(uint16_t *)value);
		    break;
	    case TEST_TYPE_3:
		    LOG("tlv_box_get_int success %d:%d \n", length, *(uint32_t *)value);
		    break;
	    case TEST_TYPE_7:
		    LOG("tlv_box_get_string success %d:%s \n", length, (char *)value);
		    break;
	    case TEST_TYPE_8:
		    LOG("tlv_box_get_bytes success %d:  ", length);
		    int i = 0;
		    for (i = 0; i < length; i++) {
			    LOG("%d-", ((uint8_t *)value)[i]);
		    }
		    LOG("\n");
            break;
        default:
            LOG("Unexpected type, test fail\n");
            return -1;
	    }
    }

    tlv_box_destroy(box);
    tlv_box_destroy(boxes);
    tlv_box_destroy(parsedBox);
    tlv_box_destroy(parsedBoxes);

    return 0;
}
