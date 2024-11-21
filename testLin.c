#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "zcan.h"

#define msleep(ms)  usleep((ms)*1000)
#define min(a,b)  (((a) < (b)) ? (a) : (b))

#define CANFD_TEST  1

#define MAX_CHANNELS  2
#define CHECK_POINT  200
#define RX_WAIT_TIME  100
#define RX_BUFF_SIZE  1000
#define SEND_NUM  8

unsigned gDevType = 0;
unsigned gDevIdx = 0;
unsigned gChMask = 0;
unsigned gTxType = 0;
unsigned gTxSleep = 0;
unsigned gTxFrames = 0;
unsigned gTxCount = 0;
unsigned gDebug = 0;

unsigned s2n(const char *s)
{
    unsigned l = strlen(s);
    unsigned v = 0;
    unsigned h = (l > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    unsigned char c;
    unsigned char t;
    if (!h) return atoi(s);
    if (l > 10) return 0;
    for (s += 2; c = *s; s++) {
        if (c >= 'A' && c <= 'F') c += 32;
        if (c >= '0' && c <= '9') t = c - '0';
        else if (c >= 'a' && c <= 'f') t = c - 'a' + 10;
        else return 0;
        v = (v << 4) | t;
    }
    return v;
}

U8 len_to_dlc(U8 len)
{
    if (len < 8) return len;
    U8 idx = ((len - 8) >> 2) & 0x0f;
    static const U8 tbl[16] = {
        8, 9, 10, 11, 12, 0,
        13, 0, 0, 0,
        14, 0, 0, 0,
        15, 0
    };
    return tbl[idx];
}

U8 dlc_to_len(U8 dlc)
{
    static const U8 tbl[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 
        8, 12, 16, 20, 24, 32, 48, 64
    };
    return tbl[dlc & 0x0f];
}

#if CANFD_TEST
void generate_frame(U8 chn, ZCAN_FD_MSG *can)
{
    U8 i, dlc = rand() % 16; // random data length£¨0~64£©
    memset(can, 0, sizeof(ZCAN_FD_MSG));
    can->hdr.inf.fmt = 1; // canfd
    can->hdr.inf.brs = 1; // 1M+4M
#else
void generate_frame(U8 chn, ZCAN_20_MSG *can)
{
    U8 i, dlc = rand() % 9; // random data length£¨0~8£©
    memset(can, 0, sizeof(ZCAN_20_MSG));
    can->hdr.inf.fmt = 0; // can2.0
    can->hdr.inf.brs = 0; // 1M+1M
#endif
    can->hdr.inf.txm = gTxType;
    can->hdr.inf.sdf = 0; // data frame
    can->hdr.inf.sef = rand() % 2; // random std/ext frame
    can->hdr.chn = chn;
    can->hdr.len = dlc_to_len(dlc);
    for (i = 0; i < can->hdr.len; i++) {
        can->dat[i] = rand() & 0x7f; // random data
        can->hdr.id ^= can->dat[i]; // id: bit0~6, checksum of data0~N
    }
    can->hdr.id |= (U32)dlc << 7; // id: bit7~bit10 = encoded_dat_len
    if (!can->hdr.inf.sef)
        return;
    can->hdr.id |= can->hdr.id << 11; // id: bit11~bit21 == bit0~bit10
    can->hdr.id |= can->hdr.id << 11; // id: bit22~bit28 == bit0~bit7
}

#if CANFD_TEST
int verify_frame(ZCAN_FD_MSG *can)
#else
int verify_frame(ZCAN_20_MSG *can)
#endif
{
    unsigned i;
    unsigned bcc = 0;
    // fixme
    //return 1;
    if (can->hdr.len > 64) return -1; // error: data length
    for (i = 0; i < can->hdr.len; i++)
        bcc ^= can->dat[i];
    if ((can->hdr.id & 0x7f) != (bcc & 0x7f)) return -2; // error: data checksum
    if (((can->hdr.id >> 7) & 0x0f) != len_to_dlc(can->hdr.len)) return -3; // error: data length
    if (!can->hdr.inf.sef) return 1; // std-frame ok
    if (((can->hdr.id >> 11) & 0x7ff) != (can->hdr.id & 0x7ff)) return -4; // error: frame id
    if (((can->hdr.id >> 22) & 0x7f) != (can->hdr.id & 0x7f)) return -5; // error: frame id
    return 1; // ext-frame ok
}

typedef struct {
    unsigned channel; // channel index, 0~3
    unsigned stop; // stop RX-thread
    unsigned total; // total received
    unsigned error; // error(s) detected
} THREAD_CTX;

void* rx_thread(void *data)
{
    THREAD_CTX *ctx = (THREAD_CTX *)data;
    ctx->total = 0; // reset counter

    ZCAN_LIN_MSG buff[RX_BUFF_SIZE]; // buffer
    int cnt; // current received
    int i;

    unsigned check_point = 0;
    while (!ctx->stop)
    {
        cnt = VCI_ReceiveLIN(gDevType, gDevIdx, ctx->channel, buff, RX_BUFF_SIZE, RX_WAIT_TIME);
        if (!cnt)
            continue;

        for (int i = 0; i < cnt; ++i)
        {
            if (0 == buff[i].dataType)
            { // 只显示LIN数据
                ZCANLINData ld = buff[i].data.zcanLINData;
                printf("[LIN %d",(int)buff[i].chnl);
                if (ld.RxData.dir == 0)
                    printf(" RX]");
                else
                    printf(" TX]");
                printf(" id: 0x %X  len:%d", ld.PID.unionVal.ID, ld.RxData.dataLen);
                printf(" data: ");
                for (int j = 0; j < 8; ++j)
                    printf(" %X ", ld.RxData.data[j]);
                printf(" time: %d\n",ld.RxData.timeStamp);
            }
        }
    }
    printf("chnl:%d thread exit \n",ctx->channel);

    pthread_exit(0);
    return NULL;
}

void* tx_thread(void *data)
{
    pthread_exit(0);
    return NULL;
}

#define CH_COUNT	2			// 通道数量
int test()
{
    // ----- device info --------------------------------------------------

    ZCAN_DEV_INF info;
    memset(&info, 0, sizeof(info));
    VCI_ReadBoardInfo(gDevType, gDevIdx, &info);
    char sn[21];
    char id[41];
    memcpy(sn, info.sn, 20);
    memcpy(id, info.id, 40);
    sn[20] = '\0';
    id[40] = '\0';
    printf("HWV=0x%04x, FWV=0x%04x, DRV=0x%04x, API=0x%04x, IRQ=0x%04x, CHN=0x%02x, LINCHN=0x%02x, SN=%s, ID=%s\n",
        info.hwv, info.fwv, info.drv, info.api, info.irq, info.chn, info.pad[0], sn, id);

    // ----- init & start -------------------------------------------------

	// 初始化并打开LIN通道，LIN0设置为主，LIN1设置为从，波特率设置为9600
	ZCAN_LIN_INIT_CONFIG LinCfg[2];
	LinCfg[0].linMode		= 1;
	LinCfg[0].linBaud = 9600;
	LinCfg[0].chkSumMode = 1;	// 经典型校验
	LinCfg[1].linMode = 0;
	LinCfg[1].linBaud = 9600;
	LinCfg[1].chkSumMode = 1;	// 经典型校验
    int ret = 0;
	for (int i = 0; i < CH_COUNT; ++i)
	{
		ret = VCI_InitLIN(gDevType, gDevIdx, i, &LinCfg[i]);
		if (ret == 0)
		{
            printf("init LIN %d failed!\n",i);
		}
		else
		{
            printf("init LIN %d succ!\n",i);
		}

		if (!VCI_StartLIN(gDevType, gDevIdx, i))
		{
			printf("start LIN %d failed!\n",i);
		}
		else
		{
			printf("start LIN %d succ!\n",i);
		}
	}

    // ----- Settings -----------------------------------------------------

    enum {
        CMD_CAN_FILTER = 0x14,
        CMD_CAN_TRES = 0x18,
        CMD_CAN_TX_TIMEOUT = 0x44,
        CMD_CAN_TTX = 0x16,
        CMD_CAN_TTX_CTL = 0x17,
        CMD_LIN_INTERNAL_RESISTANCE = 0x29, /**< 设置lin终端电阻 */
    };

    // lin terminal resistor
    U32 on = 1;
    if (!VCI_SetReference(gDevType, gDevIdx, 0, CMD_LIN_INTERNAL_RESISTANCE, &on)) {
        printf("CMD_LIN_INTERNAL_RESISTANCE failed\n");
    }

    // ----- create RX-threads --------------------------------------------

    THREAD_CTX rx_ctx[MAX_CHANNELS];
    pthread_t rx_threads[MAX_CHANNELS];
    for (int i = 0; i < MAX_CHANNELS; i++) {
        rx_ctx[i].channel = i;
        rx_ctx[i].stop = 0;
        rx_ctx[i].total = 0;
        rx_ctx[i].error = 0;
        pthread_create(&rx_threads[i], NULL, rx_thread, &rx_ctx[i]);
    }

    // ----- wait --------------------------------------------------------

    // printf("<ENTER> to start TX: %d*%d frames/channel ...\n", gTxFrames, gTxCount);
    //getchar();

    // ----- start transmit -----------------------------------------------
// 设置LIN1（从）ID响应，举例
	ZCAN_LIN_PUBLISH_CFG lpc[SEND_NUM];
	for (int i = 0; i < SEND_NUM; ++i)
	{
		lpc[i].ID = i;
		lpc[i].chkSumMode = 1;
		for (int j = 0; j < 8; ++j)
		{
			lpc[i].data[j] = i;
		}
		lpc[i].dataLen = 8;
	}
	if (!VCI_SetLINPublish(gDevType, gDevIdx, 0, &lpc, SEND_NUM))
	{
		printf("set LIN1 publish failed!\n");
	}
	// LIN0(主)发送头，ID 0 - 4
    int send_num = SEND_NUM;
	ZCAN_LIN_MSG  send_data[SEND_NUM] = {};
	for (int i = 0; i < send_num; i++)
	{
		send_data[i].dataType = 0;
		send_data[i].data.zcanLINData.PID.rawVal = i;
		send_data[i].chnl = 0;//只有主站才能发数据
	}
	int scount = VCI_TransmitLIN(gDevType, gDevIdx, 0, &send_data, send_num);//count真实发送帧数
	printf("send count : %d\n",scount);
	msleep(2000);

    // ----- stop TX & RX -------------------------------------------------

    printf("<ENTER> to stop RX ...\n");
    getchar();
    int err = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if ((gChMask & (1 << i)) == 0) continue;
        rx_ctx[i].stop = 1;
        pthread_join(rx_threads[i], NULL);
        if (rx_ctx[i].error)
            err = 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    for (int i = 0; i < 1; i++) {
    printf("test_loops=%d\n", i);
    gDevType = 41;
    gDevIdx = 0;
    if (!VCI_OpenDevice(gDevType, gDevIdx, 0)) {
        printf("VCI_OpenDevice failed\n");
        break;
    }
    printf("VCI_OpenDevice succeeded\n");

    test();

    VCI_CloseDevice(gDevType, gDevIdx);
    printf("VCI_CloseDevice\n");
    }
    return 0;
}

