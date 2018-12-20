#include <stdio.h>
#include <stdlib.h>
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

#define NORMAL_NO_ID 0

#define SUCCESS  0
#define FAIL    -1

//#define LOG_PRINT


int print_each_tlv(struct tlv_box *parsebox)
{
	struct tlv *tlv;
	uint16_t type;
	uint16_t length;
	void *value;
	int res;

	tlv_box_for_each_tlv (parsebox, tlv, type, length, value) {
	 	switch (type) {
		case TEST_TYPE_3:
			#ifdef LOG_PRINT
			printf("get data success and len=%d!!\n", length);
			for(int i=0; i<length; i++)
				printf("value=%d\t ", ((char*)value)[i]);
			printf("\n");
			#endif
			break;
		case TEST_TYPE_6:
			#ifdef LOG_PRINT
			printf("get data success and len=%d:value=%s\n", length, (char *)value);
			#endif
			break;
		default:
			printf("test fail !!!\n");
			return FAIL;
		}
	 }
//	tlv_box_print(parsebox);
	return SUCCESS;
}



int  put_raw_no_id_test(struct tlv_box *box1, struct tlv_box *parsebox1, int flag)
{
#ifdef LOG_PRINT
	printf("box->how=%d\n", box1->how);
	printf("parsebox->how=%d\n", parsebox1->how);
#endif

	unsigned char brr[10] = {1,2,3,4,5,6,7,8,9,10};
	int brr_size = sizeof(brr)/sizeof(brr[0]);
	struct message msg = {
				.len = 10, 
				.data = brr,
			     };
	tlv_box_put_raw(box1, TEST_TYPE_3, &msg, flag & 0);

	char str[] = "try the tlv test!!!";
	tlv_box_put_string(box1, TEST_TYPE_6, str, flag & 0);

	char *p = (char*)malloc(sizeof(char)*5);
	for(int i=0; i<5; ++i) {
		p[i]=i+10;
	}
	struct message msg1 = {
				.len = 5,
				.data = p,
			      };
	tlv_box_put_raw(box1, TEST_TYPE_3, &msg1, flag);

	if (tlv_box_serialize(box1) != SUCCESS) 
		return FAIL;

	int res1 = tlv_box_parse(parsebox1, tlv_box_get_buffer(box1), tlv_box_get_size(box1));
	if (res1 != 0) 
		return FAIL;
	print_each_tlv(parsebox1);	 
	return SUCCESS;
}

int  raw_about_id_flag_test(int id, int flag)
{
	printf("\n");
	printf("----------------tlv_box_put_raw no_id test   how=%d---------------------\n", id);
	struct tlv_box *box1 = tlv_box_create(id);
	struct tlv_box *parsebox1 = tlv_box_create(id);
	
	if( put_raw_no_id_test(box1, parsebox1, flag) != SUCCESS)
		return FAIL;
	tlv_box_destroy(parsebox1);
	tlv_box_destroy(box1);
	return SUCCESS;
}

int put_raw_test(int flag)
{
	if(raw_about_id_flag_test(NORMAL_NO_ID, flag) != SUCCESS)
		return FAIL;
	if(raw_about_id_flag_test(SERIAL_WITH_ID, flag) != SUCCESS)
		return FAIL;
	if(raw_about_id_flag_test(SERIAL_EACH_WITH_ID, flag) != SUCCESS)
		return FAIL;
	printf("put_raw_test three tests for ids success!!\n");
	return SUCCESS;
}

int put_box_no_id_test(struct tlv_box *box1, struct tlv_box *boxs, struct tlv_box *parseboxs, struct tlv_box *pbox, int flag)
{

	struct tlv *tlv;
	uint16_t type;
	uint16_t length;
	void *value;
	int res;

#ifdef LOG_PRINT
	printf("box1->how=%d\n", box1->how);
	printf("boxs->how=%d\n", boxs->how);
	printf("parsebox1->how=%d\n", parseboxs->how);
	printf("box1->how=%d\n", pbox->how);
#endif

	struct tlv_box *box2 = tlv_box_create(boxs->how);
	unsigned char brr[10] = {1,2,3,4,5,6,7,8,9,10};
	int brr_size = sizeof(brr)/sizeof(brr[0]);
	struct message msg = {
				.len = 10,
				.data = brr,
			     };
	tlv_box_put_raw(box1, TEST_TYPE_3, &msg, flag & 0);
	
	char str[] = "try the tlv test!!!";
	tlv_box_put_string(box1, TEST_TYPE_6, str, flag & 0);
	
	char *p = (char*)malloc(sizeof(char)*5);
	for(int i=0; i<5; ++i){
		p[i]=i+10;
	}
	struct message msg1 = {
				.len = 5,
				.data = p,
			      };
	tlv_box_put_raw(box2, TEST_TYPE_3, &msg1, TLV_NOFREE);

	char str1[] = "try to do tlv test";
	tlv_box_put_string(box2, TEST_TYPE_6, str1, flag & 0);

	tlv_box_put_box(boxs, TEST_TYPE_3, box2);
	tlv_box_put_box(boxs, TEST_TYPE_2, box1);

	if(tlv_box_serialize(boxs) != SUCCESS)
		return FAIL;

	res = tlv_box_parse(parseboxs, tlv_box_get_buffer(boxs), tlv_box_get_size(boxs));
	if (res != SUCCESS)
		return FAIL;

	 tlv_box_for_each_tlv (parseboxs, tlv, type, length, value) {
		switch(tlv->type) {
		case TEST_TYPE_2:
		 	if(tlv_box_parse(pbox,tlv->value,tlv->length) != SUCCESS)
				return FAIL;
			break;
		case TEST_TYPE_3:
		 	if(tlv_box_parse(pbox,tlv->value,tlv->length) != SUCCESS)
				return FAIL;
			break;
		default:
			return FAIL;
		}
	 }
	print_each_tlv(pbox);
	free(p);
	tlv_box_destroy(box2);
	return SUCCESS;
}

int box_about_id_flag_test(int how, int flag)
{
	struct tlv *tlv;
	uint16_t type;
	uint16_t length;
	void *value;

	printf("\n");
	printf("--------------------------tlv_box_put_box test how=%d------------------------------\n", how);
	struct tlv_box *box1 = tlv_box_create(how);
	struct tlv_box *boxs = tlv_box_create(how);
	struct tlv_box *parseboxs = tlv_box_create(how);
	struct tlv_box *pbox = tlv_box_create(how);

	if(put_box_no_id_test(box1, boxs, parseboxs, pbox, flag) != SUCCESS)
		return FAIL;

	tlv_box_destroy(box1);
	tlv_box_destroy(parseboxs);
	tlv_box_destroy(pbox);
	tlv_box_destroy(boxs);
	return SUCCESS;
}

int put_box_test(int flag)
{
	if(box_about_id_flag_test(NORMAL_NO_ID, flag) != SUCCESS)
		return FAIL;
	if(box_about_id_flag_test(SERIAL_WITH_ID, flag) != SUCCESS)
		return FAIL;
	if(box_about_id_flag_test(SERIAL_EACH_WITH_ID, flag) != SUCCESS)
		return FAIL;
	printf("put_box_test three tests for ids sucess!!\n");
	return SUCCESS;
}

int main(int argc, char const *argv[])
{	
	int flag = 0;
	printf("********************************************flag = 0******************************************\n");
	put_raw_test(flag);
	put_box_test(flag);


	printf("************************************testing for TLV_NOCPY**************************************\n");
	flag = TLV_NOCPY;
	put_raw_test(flag);
	put_box_test(flag);

	printf("over!!!\n");
	return 0;
}
