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
	struct tlv *tlv;
	uint16_t type;
	uint16_t length;
	void *value;
	int res;
	struct tlv_box *box1 = tlv_box_create();
	struct tlv_box *box2 = tlv_box_create();
	tlv_box_init(box1);
	tlv_box_init(box2);
	

	unsigned char arr[10] = {1,2,3,4,5,6,7,8,9,10};
	tlv_box_put_raw(box1, TEST_TYPE_3, 10, arr);

	char str[] = "try the tlv test!!!";
	tlv_box_put_string(box1, TEST_TYPE_6, str);

	if(tlv_box_serialize(box1) == 0)
	{
		printf("tlv_box_serialize success\n");
	}
	else{
		printf("tlv_box_serialize failed\n");
		return -1;
	}

	 res=tlv_box_parse(box2,tlv_box_get_buffer(box1), tlv_box_get_size(box1));
	 if(res==0)
	 {
	   	printf("tlv_box_parse ok!!!\n");
	 }
	 else
	 {
	 	printf("tlv_box_parse failed!!\n");
	 }
	
	 tlv_box_for_each_tlv(box2, tlv, type, length, value)
	 {
	 	switch(type){
		case TEST_TYPE_3:
			printf("get data success and len=%d\n",length);
			for(int i=0;i<length;i++)
			{
				printf("value=%d\t ",((char *)value)[i]);
			}
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
	 

	tlv_box_destroy(box2);
	tlv_box_destroy(box1);
	
	printf("over!!!\n");
	return 0;
}
