#include "zcan.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"

#define CANFD_TEST  1

#define MAX_CHANNELS  4
#define CHECK_POINT  200
#define RX_WAIT_TIME  100
#define RX_BUFF_SIZE  1000

unsigned gDevType = 33;
unsigned gDevIdx = 0;
unsigned gChMask = 3;
unsigned gDataNum = 0; 

static void can_start() { 
    ZCAN_INIT init;
    init.clk = 60000000; // clock: 60M
    init.mode = 0;
#if CANFD_TEST
    init.aset.tseg1 = 46; // 1M
    init.aset.tseg2 = 11;
    init.aset.sjw = 3;
    init.aset.smp = 0;
    init.aset.brp = 0;
    init.dset.tseg1 = 10; // 4M
    init.dset.tseg2 = 2;
    init.dset.sjw = 2;
    init.dset.smp = 0;
    init.dset.brp = 0;
#else
    init.aset.tseg1 = 46; // 1M
    init.aset.tseg2 = 11;
    init.aset.sjw = 3;
    init.aset.smp = 0;
    init.aset.brp = 2;
    init.dset.tseg1 = 14; // 1M
    init.dset.tseg2 = 3;
    init.dset.sjw = 3;
    init.dset.smp = 0;
    init.dset.brp = 0;
#endif

    int i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if ((gChMask & (1 << i)) == 0) continue;

        if (!VCI_InitCAN(gDevType, gDevIdx, i, &init)) {
            printf("VCI_InitCAN(%d) failed\n", i);
        }
        printf("VCI_InitCAN(%d) succeeded\n", i);

        if (!VCI_StartCAN(gDevType, gDevIdx, i)) {
            printf("VCI_StartCAN(%d) failed\n", i);
        }
        printf("VCI_StartCAN(%d) succeeded\n", i);
    }

    enum {
          CMD_CAN_FILTER = 0x14,
          CMD_CAN_TRES = 0x18,
          CMD_CAN_TX_TIMEOUT = 0x44,
          CMD_CAN_TTX = 0x16,
          CMD_CAN_TTX_CTL = 0x17,
    };
  
    // transmit timeout
    U32 tx_timeout = 2000; // 2 seconds
    if (!VCI_SetReference(gDevType, gDevIdx, 0, CMD_CAN_TX_TIMEOUT, &tx_timeout)) {
        printf("CMD_CAN_TX_TIMEOUT failed\n");
    }

    // terminal resistor
    U32 on = 1;
    if (!VCI_SetReference(gDevType, gDevIdx, 0, CMD_CAN_TRES, &on)) {
        printf("CMD_CAN_TRES failed\n");
    }
}

void test_uds() { 
    ZCAN_UDS_REQUEST request;  
    ZCAN_UDS_RESPONSE resp;
    uint8_t resp_data[4096];
    static uint8_t data[0x10000];

    request.req_id = 0;
    request.channel = 1;
    request.frame_type = ZCAN_UDS_FRAME_CANFD;
    request.src_addr = 0x731;
    request.dst_addr = 0x7B1;
    request.suppress_response = 1;
    request.sid = 0x34;
    request.session_param.timeout = 1000;
    request.session_param.enhanced_timeout = 1000;
    request.session_param.check_any_negative_response = 1;
    request.session_param.wait_if_suppress_response = 0;

    request.trans_param.version = ZCAN_UDS_TRANS_VER_1;
    request.trans_param.max_data_len = 64;
    request.trans_param.local_st_min = 0;
    request.trans_param.block_size= 0;
    request.trans_param.fill_byte = 0;
    request.trans_param.ext_frame = 0;
    request.trans_param.is_modify_ecu_st_min = 0;
    request.trans_param.remote_st_min = 0;
    request.trans_param.fc_timeout = 1000;
    request.data = data;
    request.data_len = gDataNum;
    if (request.data_len >sizeof(data)) {
        request.data_len = sizeof(data); 
    }

    for (uint32_t i = 0; i < gDataNum; i++) {
        data[i] = (uint8_t)i;
    }

    int ret = VCI_UDS_Request(gDevType, gDevIdx, &request, &resp, resp_data, sizeof(resp_data));
    printf("uds result:%d\n", ret);
    if (ret) {
        printf("resp:%d, type:%d\n", resp.status, resp.type);
        printf("respone data:");
        for (uint32_t i = 0; i < resp.positive.data_len; i++) {
            printf("%02x ", resp_data[i]);
        }
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        gDataNum = atoi(argv[1]); 
    }
    printf("Test Uds data num:%d\n", gDataNum);
    if (!VCI_OpenDevice(gDevType, gDevIdx, 0)) {
        printf("VCI_OpenDevice failed\n");
        return -1;
    }

    printf("VCI_OpenDevice succeeded\n");

    can_start();
    test_uds();

    VCI_CloseDevice(gDevType, gDevIdx);
    printf("VCI_CloseDevice\n");
    return 0;
}
