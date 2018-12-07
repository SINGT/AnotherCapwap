#include <stdio.h>
#include <string.h>
#include "../tlv.h"

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

//	printf("----------------tlv_box_put_raw no_id test   how=0---------------------\n");
//	printf("----------------tlv_box_put_raw SERIAL_WITH_ID test  how=1-------------\n");
//	printf("----------------tlv_box_put_raw SERIAL_EACH_WITH_ID how=2--------------\n");

	printf("----------------tlv_box_put_box no_id test how=0------------------------\n");
//	printf("----------------tlv_box_put_box SERIAL_WITH_ID test how=1---------------\n");
//	printf("----------------tlv_box_put_box SERIAL_EACH_WITH_ID test how=2----------\n");
	struct tlv *tlv;
	uint16_t type;
	uint16_t length;
	void *value;
	struct tlv_box *box1 = tlv_box_create();
//	struct tlv_box *parsebox1 = tlv_box_create();
	struct tlv_box *boxs = tlv_box_create();
	struct tlv_box *parseboxs = tlv_box_create();
	struct tlv_box *pbox = tlv_box_create();

	tlv_box_init(box1);
//	tlv_box_init(parsebox1);
	tlv_box_init(boxs);
	tlv_box_init(parseboxs);
	tlv_box_init(pbox);

	tlv_box_set_how(box1, SERIAL_WITH_ID);
//	tlv_box_set_how(parsebox1, SERIAL_WITH_ID);
	tlv_box_set_how(boxs, SERIAL_WITH_ID);
	tlv_box_set_how(pbox, SERIAL_WITH_ID);
//	tlv_box_set_how(box1, SERIAL_EACH_WITH_ID);
//	tlv_box_set_how(parsebox1, SERIAL_EACH_WITH_ID);
//	tlv_box_set_how(boxs, SERIAL_EACH_WITH_ID);
	
	printf("box1->how=%d\n", box1->how);
	printf("box1->how=%d\n", boxs->how);
//	printf("parsebox1->how=%d\n", parsebox1->how);
	printf("parsebox1->how=%d\n", parseboxs->how);
	printf("box1->how=%d\n", pbox->how);
	
	unsigned char arr[10] = {1,2,3,4,5,6,7,8,9,10};
	tlv_box_put_raw(box1, TEST_TYPE_3, 10, arr);

	char str[] = "try the tlv test!!!";
	tlv_box_put_string(box1, TEST_TYPE_6, str);

	int res1=tlv_box_serialize(box1);
	if (res1 == 0) {
		printf("box1->serialized_len=%d\n",box1->serialized_len);
		printf("tlv_box_serialize box1 success\n");
	}
	else {
		printf("tlv_box_serialize box1 failed\n");
		return -1;
	}

/*
	printf("tlv_box_serialize  box->len=%d!\n",box1->serialized_len);
     int res2 = tlv_box_parse(parsebox1,tlv_box_get_buffer(box1), tlv_box_get_size(box1));
	 if (res2 == 0)
	   	printf("tlv_box_parse ok!!!\n");
	 else
	 	printf("tlv_box_parse failed!!\n");
*/


	tlv_box_put_box(boxs,TEST_TYPE_2,box1);
	if(tlv_box_serialize(boxs) == 0)
		printf("tlv_box_serialize boxs success\n");
	else
		printf("tlv_box_serialize boxs failed\n");

	int res3 = tlv_box_parse(parseboxs,tlv_box_get_buffer(boxs),tlv_box_get_size(boxs));
	if (res3 == 0)
		printf("tlv_box_parse boxs ok!!!\n");
	else
		printf("tlv_box_parse boxs failed!!!\n");


	 tlv_box_for_each_tlv (parseboxs, tlv, type, length, value) {
		 switch(tlv->type) {
		 case TEST_TYPE_2:
		 	if(tlv_box_parse(pbox,tlv->value,tlv->length) == 0 )
				printf("tlv_box_parse pbox success\n");
		 	else
				printf("tlv_box_parse pbox failed\n");
			break;
		default:
			return -1;
		 }
	 }


//	 tlv_box_for_each_tlv (parsebox1, tlv, type, length, value) {
	 tlv_box_for_each_tlv (pbox, tlv, type, length, value) {
	 	switch (type) {
		case TEST_TYPE_3:
			printf("get data success and len=%d\n",length);
			for(int i=0;i<length;i++)
				printf("value=%d\t ",((char *)value)[i]);
			printf("\n");
			break;
		case TEST_TYPE_6:
			printf("get data success and len=%d:value=%s\n",length, (char *)value);
			break;
		default:
			printf("test fail !!!\n");
			return -1;
		}
	 }
	 

//	tlv_box_destroy(parsebox1);
	tlv_box_destroy(box1);
	tlv_box_destroy(parseboxs);
	tlv_box_destroy(pbox);
	tlv_box_destroy(boxs);
	printf("over!!!\n");
	return 0;
}
