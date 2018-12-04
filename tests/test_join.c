#include "../ac_join.c"

int main(int argc, char const *argv[])
{
	struct capwap_wtp wtp = {0};
	struct cw_ctrlmsg *join;
	struct tlv_box *box = tlv_box_create();
	int err;

	join = cwmsg_ctrlmsg_malloc();
	cwmsg_ctrlmsg_add_element(join, CW_MSG_ELEMENT_LOCATION_DATA_CW_TYPE, 10, "Near desk");


	// cwmsg_ctrlmsg_add_element(join, CW_MSG_ELEMENT_WTP_BOARD_DATA_CW_TYPE, );

	return 0;
}
