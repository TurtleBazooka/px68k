// ---------------------------------------------------------------------
//  SCSI.C - 外付けSCSIボード (CZ-6BS1) 
//  ea0001~ea001f SPC
//  SRAM ed006f="V"  Exp:V,0f,00
//       ed0070 0x07(Own-ID), bit3:1=CZ-6BS1 0=internal
//       ed0071 SASI-Flg 
//  SCSI-iocs level emuration, need IPL "scsiexrom.dat"
// ---------------------------------------------------------------------

#include	"common.h"
#include	"x68kmemory.h"
#include	"dosio.h"
#include	"prop.h"
#include	"winx68k.h"
#include	"scsi.h"
#include	"../m68000/m68000.h"
//#include	"../m68000/c68k/c68k.h"

// ----------------------
//   define
// ----------------------

/*SPC command*/
#define	scmd_spc_busr	0x00 /*Bus Release*/
#define	scmd_spc_sel	0x20 /*Select*/
#define	scmd_spc_ratn	0x40 /*reset ATN*/
#define	scmd_spc_satn	0x60 /*Set ATN*/
#define	scmd_spc_trns	0x80 /*Transfer*/
#define	scmd_spc_trnsp	0xa0 /*Transfer Pause*/
#define	scmd_spc_rstack	0xc0 /*Reset ACK/REQ*/
#define	scmd_spc_setack	0xe0 /*Set ACK/REQ*/

/* INTS bit (exception status) */
#define	ints_spc_rst	0x01 /*Reset Condition*/
#define	ints_spc_err	0x02 /*SPC Hard Error*/
#define	ints_time_out	0x04 /*Time Out*/
#define	ints_srvc_req	0x08 /*Service Required*/
#define	ints_cmd_comp	0x10 /*Command Complete*/
#define	ints_scsi_dscn	0x20 /*Disconnected SCSI bus*/
#define	ints_scsi_resel	0x40 /*reselection phase*/
#define	ints_scsi_sel	0x80 /*selection phase*/

/* PSNS bit (REQ ACK ATN SEL BSY MSG C/D I/O) */
#define	psns_scsi_io	0x01
#define	psns_scsi_cd	0x02
#define	psns_scsi_msg	0x04
#define	psns_scsi_bsy	0x08
#define	psns_scsi_sel	0x10
#define	psns_scsi_atn	0x20
#define	psns_scsi_ack	0x40
#define	psns_scsi_req	0x80

/* SDGC bit*/
#define	sdgc_xfer_enbl	0x20 /*transfer exception enable*/

/* SSTS bit*/
#define	ssts_dreg_emp	0x01 /*DREG status*/
#define	ssts_dreg_ful	0x02
#define	ssts_cnt_zero	0x04 /*Transfer counter ZERO*/
#define	ssts_rst_low	0x08 /*SCSI ResetIn*/
#define	ssts_trs_prog	0x10 /*Transfer in progress*/
#define	ssts_spc_bsy	0x20 /*SPC Busy*/
#define	ssts_conn_tgt	0x40 /*Connected status*/
#define	ssts_conn_ini	0x80

/* SERR bit*/
#define	serr_trns_prod	0x02 /*Short Transfer Period*/
#define	serr_tc_perr	0x04 /*TC Parity Error*/
#define	serr_xfer_out	0x20 /*Xfer Out*/

#define	serr_perr_out	0x40 /*Data Error SCSI out*/
#define	serr_perr_in	0xc0 /*Data Error SCSI in */

/* PCTL transfer phase bit*/
#define	pctl_data_out	0x00
#define	pctl_data_in	0x01
#define	pctl_cmd		0x02
#define	pctl_status		0x03
#define	pctl_msg_out	0x06
#define	pctl_msg_in		0x07
#define	pctl_int_enbl	0x80 /*Busfree INT Enable*/

/* SCTL transfer phase bit*/
#define	sctl_int_enbl	0x01
#define	sctl_resel_enbl	0x02
#define	sctl_sel_enbl	0x04
#define	sctl_prty_enbl	0x08
#define	sctl_arb_enbl	0x10
#define	sctl_diag_mode	0x20
#define	sctl_cont_rst	0x40
#define	sctl_rst_disabl	0x80

/*SCSI Phase NO.*/
#define	SCSI_Phase_busfree 0
#define	SCSI_Phase_command 1
#define	SCSI_Phase_status 2
#define	SCSI_Phase_datain 3
#define	SCSI_Phase_dataout 4
#define	SCSI_Phase_messagein 5
#define	SCSI_Phase_messageout 6

void Para_Reset(void);
void Bus_Reset(void);
void init_SPCreg();

uint8_t SetPSNS();
uint8_t SetSSTS();

uint32_t Get_FileSize(uint8_t SCSI_ID);
BOOL SCSI_BlockRead(void);
BOOL SCSI_BlockWrite(void);

/*MB89352 reg set*/
static uint8_t	scsi_bdid;
static uint8_t	scsi_sctl;
static uint8_t	scsi_scmd;
static uint8_t	scsi_ints;
static uint8_t	scsi_psns;
static uint8_t	scsi_sdgc;
static uint8_t	scsi_ssts;
static uint8_t	scsi_serr;
static uint8_t	scsi_pctl;
static uint8_t	scsi_mbc;
static uint8_t	scsi_dreg;
static uint8_t	scsi_temp;
static uint32_t	scsi_TrCnt;

/* SCSI Bus Signal */
static uint8_t	scsi_bsy = 0;
static uint8_t	scsi_sel = 0;
static uint8_t	scsi_atn = 0;
static uint8_t	scsi_msg = 0;
static uint8_t	scsi_cd  = 0;
static uint8_t	scsi_io  = 0;
static uint8_t	scsi_req = 0;
static uint8_t	scsi_ack = 0;

static uint8_t	scsi_rst = 0;
static uint8_t	scsi_len = 0;

/*SCSI para.*/
static int32_t	scsi_phase;
static int32_t	SCSI_TrPhase;
static int32_t	SCSI_Blocks;
static int32_t	SCSI_BlockSize;
static int32_t	SCSI_CmdPtr;
static int32_t	SCSI_Device;
static int32_t	SCSI_Unit;
static int32_t	SCSI_BufPtr;
static int32_t	SCSI_RW;
static int32_t	SCSI_Stat;
static int32_t	SCSI_Error;
static int32_t	SCSI_SenseStatPtr;

static int32_t	scsi_trans;

	uint8_t SCSIIPL[0x2000];
	uint8_t SCSI_Buf[2048];

// ----------------------------
//   ALL 初期化
// ----------------------------
void SCSI_Init(void)
{
	init_SPCreg();
	Para_Reset();
	Bus_Reset();

return;
}

// -----------------------------
//   SCSI Para. reset
// -----------------------------
void Para_Reset(void)
{

	scsi_phase = SCSI_Phase_busfree;
	SCSI_BlockSize = 512;
	SCSI_Blocks = 0;
	SCSI_CmdPtr = 0;
	SCSI_Device = 0;
	SCSI_Unit = 0;
	SCSI_BufPtr = 0;
	SCSI_RW = 0;
	SCSI_Stat = 0;
	SCSI_Error = 0;
	SCSI_SenseStatPtr = 0;

return;
}
// -----------------------------
//   SCSI BUS Reset 
// -----------------------------
void Bus_Reset()
{

	scsi_trans = 0; /*転送強制終了*/

	/* SCSI Bus negative */
	scsi_bsy = 0;
	scsi_sel = 0;
	scsi_atn = 0;
	scsi_msg = 0;
	scsi_cd  = 0;
	scsi_io  = 0;
	scsi_req = 0;
	scsi_ack = 0;

	scsi_rst = 0;
	scsi_len = 0;

	scsi_phase = SCSI_Phase_busfree; /*busfree*/

  return;
}


// ---------------------------------
//   init MB89352 register
// ---------------------------------

void init_SPCreg()
{
	scsi_bdid = 0;
	scsi_sctl = 0;
	scsi_scmd = 0;
	scsi_ints = 0;
	scsi_psns = 0;
	scsi_sdgc = 0;
	scsi_ssts = 0;
	scsi_serr = 0;
	scsi_pctl = 0;
	scsi_mbc = 0;
	scsi_dreg = 0;
	scsi_temp = 0;
	scsi_TrCnt = 0;

}

// ---------------------------------------------------
//   撤収〜
// ---------------------------------------------------
void SCSI_Cleanup(void)
{
}

/* 代替SCSI-IOCS */
void
SCSI_iocs(uint8_t SCSIiocs)
{
 uint32_t i,j;

 uint32_t target_id;
 uint32_t m68_adrs;
 uint32_t dtcnt;
 uint8_t RQ_SENSE[8];
 uint32_t INQ_blksize[8];
 uint32_t INQ_blktotal[8];

 static uint8_t CZ_6MO1[]={1,1,1,1,1,1,1,1}; /*Eject or insert*/

 static uint8_t SCSI_OK[]={1,1,1,1,1,1,1,1}; /*0:操作不可能 1:操作可能*/

 static uint8_t INQHD[] = {0x00,0x80,0x02,0x02,19,0x00,0x01,0x3e,
			'H','D','D',' ','I','M','A','G','E',' ','P','X','6','8','K'};
 static uint8_t INQMO[] = {0x07,0x80,0x02,0x02,23,0x00,0x00,0x10,
			'F','U','J','I','T','S','U',' ','M','C','R','3','2','3','0','S','S','-','S'};
 static uint8_t MAGICSTR[] = {'X','6','8','S','C','S','I','1',0x02,00, 00,00,00,00 ,0x01,0x00};

 switch(SCSIiocs)
 {
	case 0x00:/*SPC のリセット及び SCSI バスのリセット*/
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x01:/*アービトレーションフェーズとセレクションフェーズの実行*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x02:/*アービトレーションフェーズとセレクションフェーズの実行(ATN)*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		//scsi_atn = TRUE;
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x03:/*コマンドアウトフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x04:/*データインフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x05:/*データアウトフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x06:/*ステータスインフェーズの実行*/
		/*00:Good 02:chkCondition 04:ConditionMet 08:Busy 10:Intermediate 14:Intermediate ConditionMet*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x07:/*メッセージインフェーズの実行*/
		/*00: CommandComplete 01:ExtendedMessage 02:OneByteMessage 20:2ByteMessage*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x08:/*メッセージアウトフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x09:/*フェーズセンス*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x0a:/*SCSI IOCS のバージョンを調べる*/
		//m68000_set_reg(M68K_D0, 0x00);/*X68 Super*/
		//m68000_set_reg(M68K_D0, 0x01);/*CZ-6BS1*/
		//m68000_set_reg(M68K_D0, 0x03);/*X68 XVI*/
		m68000_set_reg(M68K_D0, 0x10);/*X68030 */
	 break;
	case 0x0b:/*データインフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x0c:/*データアウトフェーズの実行*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	case 0x0d:/*メッセージアウトフェーズの実行*/
		m68000_set_reg(M68K_D0, 0x11);/*NG(未サポート)*/
	 break;
	case 0x20:/*INQUIRY データの要求*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68_adrs  = m68000_get_reg(M68K_A1);
		dtcnt     = m68000_get_reg(M68K_D3);
		SCSI_Device = target_id;
		SCSI_Blocks = 0;
		SCSI_BlockSize = 512;
		if(SCSI_BlockRead()==TRUE){
			for(i=0; i<dtcnt; i++){
			  Memory_WriteB(m68_adrs+i, INQHD[i]);
			}
			m68000_set_reg(M68K_D0,0);/*OK*/
		}
		else{
			if(target_id == 5){ //MO Drive
			  for(i=0; i<dtcnt; i++){
			   Memory_WriteB(m68_adrs+i, INQMO[i]);
			  }
			  m68000_set_reg(M68K_D0,0);/*OK*/
			}
			else{
			  m68000_set_reg(M68K_D0, 0x11);/*No device*/
			}
		}
	 break;
	case 0x21:/*SCSI 装置よりデータの読み込み*/
	case 0x26:/*拡張 Read*/
	case 0x2e:/*DMA Read*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68_adrs  = m68000_get_reg(M68K_A1);
		dtcnt     = m68000_get_reg(M68K_D3);/*読み込みブロック数*/
		SCSI_Device = target_id;
		SCSI_Blocks = m68000_get_reg(M68K_D2);
		switch(m68000_get_reg(M68K_D5))
		{
		  case 0: SCSI_BlockSize =  256; break;
		  case 1: SCSI_BlockSize =  512; break;
		  case 2: SCSI_BlockSize = 1024; break;
		  default:SCSI_BlockSize =  512; break;
		}
		for(i=0; i<dtcnt; i++){
		  if(SCSI_BlockRead()==TRUE){
			for(j=0; j<SCSI_BlockSize; j++){
			  Memory_WriteB(m68_adrs+(i*SCSI_BlockSize)+j,SCSI_Buf[j]);
			}
			m68000_set_reg(M68K_D0,0);/*OK*/
		  }
		  else{
			m68000_set_reg(M68K_D0, 0x11);/*err*/
			break;
		  }
		  SCSI_Blocks++;
		}
	 break;
	case 0x22:/*SCSI 装置へのデータの書き込み*/
	case 0x27:/*拡張 Write*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68_adrs  = m68000_get_reg(M68K_A1);
		dtcnt     = m68000_get_reg(M68K_D3);/*読み込みブロック数*/
		SCSI_Device = target_id;
		SCSI_Blocks = m68000_get_reg(M68K_D2);
		switch(m68000_get_reg(M68K_D5))
		{
		  case 0: SCSI_BlockSize =  256; break;
		  case 1: SCSI_BlockSize =  512; break;
		  case 2: SCSI_BlockSize = 1024; break;
		  default:SCSI_BlockSize =  512; break;
		}
		for(i=0; i<dtcnt; i++){
		  for(j=0; j<SCSI_BlockSize; j++){
		    SCSI_Buf[j] = Memory_ReadB(m68_adrs+(i*SCSI_BlockSize)+j);
		  }
		  if(SCSI_BlockWrite()==TRUE){
			m68000_set_reg(M68K_D0,0);/*OK*/
		  }
		  else{
			m68000_set_reg(M68K_D0, 0x11);/*err*/
			break;
		  }
		  SCSI_Blocks++;
		}
	 break;
	case 0x23:/*SCSI 装置のフォーマット*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		SCSI_Device = target_id;
		SCSI_Blocks = 0;
		SCSI_BlockSize = 512;
		if(target_id == (Memory_ReadB(0xed001b) & 0x07))/*OWN-ID check*/
			m68000_set_reg(M68K_D0, 0xffffffff);/*err(not support)*/

		memset(SCSI_Buf, 0, SCSI_BlockSize);
		memcpy(SCSI_Buf,MAGICSTR,sizeof(MAGICSTR));
		if((SCSI_BlockWrite()==TRUE)&&(SCSI_OK[target_id]==TRUE)){
			m68000_set_reg(M68K_D0,0);/*OK*/
		}
		else{
			m68000_set_reg(M68K_D0, 0xffffffff);/*err(not support)*/
		}
	 break;
	case 0x24:/*SCSI 装置が動作可能であるか調べる*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		SCSI_Device = target_id;
		SCSI_Blocks = 0;
		SCSI_BlockSize = 512;
		if(target_id == (Memory_ReadB(0xed001b) & 0x07)){/*OWN-ID check*/
			m68000_set_reg(M68K_D0, 0xffffffff);/*err(not support)*/
		}
		else{
		  if((SCSI_BlockRead()==TRUE)&&(SCSI_OK[target_id]==TRUE)){
			m68000_set_reg(M68K_D0,0);/*OK*/
			//printf("DeviceNO %d OK\n",SCSI_Device);
		  }
		  else{
			m68000_set_reg(M68K_D0, 0x11);/*No device*/
		  }
		}
	 break;
	case 0x25:/*SCSI 装置の容量に関する情報を調べる*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68_adrs  = m68000_get_reg(M68K_A1);
		SCSI_Device = target_id;
		SCSI_Blocks = 0;
		SCSI_BlockSize = 512;
		if(SCSI_BlockRead()==TRUE){
			//if(memcmp(SCSI_Buf,"X68SCSI1",8) == 0){
			//  i = (SCSI_Buf[0x0a]<<24)|(SCSI_Buf[0x0b]<<16)|(SCSI_Buf[0x0c]<<8)|SCSI_Buf[0x0d];/*全ブロック数*/
			//  j = (SCSI_Buf[8]<<8 | SCSI_Buf[9]);
			//}
			//else{
			  i= (Get_FileSize(SCSI_Device)/SCSI_BlockSize);
			  j= SCSI_BlockSize;
			//}
			INQ_blktotal[target_id]=i;
			Memory_WriteD(m68_adrs, i);
			INQ_blksize[target_id]=j;
			Memory_WriteD(m68_adrs+4, j);
			m68000_set_reg(M68K_D0,0);/*OK*/
		}
		else{
			m68000_set_reg(M68K_D0, 0x11);/*No device*/
		}
	 break;
	case 0x28:/*拡張 VERIFY コマンド*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68000_set_reg(M68K_D0,0);/*OK(DUMMY)*/
	  break;
	case 0x29:/*MODE SENSE データの要求*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68000_set_reg(M68K_D0,0);/*OK(DUMMY)*/
	  break;
	case 0x2a:/*MODE SELECT コマンド*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68000_set_reg(M68K_D0,0);/*OK(DUMMY)*/
	  break;
	case 0x2b:/*SCSI 装置を指定の状態にセット*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68000_set_reg(M68K_D0,0);/*OK(DUMMY)*/
	  break;
	case 0x2c:/*SCSI 装置のセンスデータを調べる*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		m68_adrs  = m68000_get_reg(M68K_A1);
		dtcnt     = m68000_get_reg(M68K_D3);/*読み込みbyte数*/
		SCSI_Device = target_id;
		SCSI_Blocks = 0;
		SCSI_BlockSize = 512;
		if(SCSI_BlockRead()==TRUE){
			RQ_SENSE[0]=0x80;//Losical Address OK
			RQ_SENSE[1]=SCSI_Buf[11]; RQ_SENSE[2]=SCSI_Buf[12];RQ_SENSE[3]=SCSI_Buf[13];
			for (i=0; i<dtcnt; i++){
			  Memory_WriteB(m68_adrs+i, RQ_SENSE[i]);
			}
			m68000_set_reg(M68K_D0,0);/*OK*/
		}
		else{
			m68000_set_reg(M68K_D0, 0x11);/*No device*/
		}
	 break;
	case 0x2d:/*指定の論理ブロックアドレスへシークする*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		SCSI_Device = target_id;
		SCSI_Blocks = m68000_get_reg(M68K_D2);
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x2f:/*SCSI 装置に対して以降の操作を可能・不可能にすることを要求*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		dtcnt     = m68000_get_reg(M68K_D3);/*(0:操作不可能 1:操作可能 2:メディア排出)*/
		if(dtcnt == 0) SCSI_OK[target_id] = FALSE;
		if(dtcnt == 1) SCSI_OK[target_id] = TRUE;
		if(dtcnt == 2) CZ_6MO1[target_id] = FALSE;
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x30:/*MO 排出*/
		target_id = m68000_get_reg(M68K_D4) & 0xff;
		CZ_6MO1[target_id] = FALSE;
		m68000_set_reg(M68K_D0,0);/*OK*/
	 break;
	case 0x31:/*欠陥ブロックの再割り当て*/
	case 0x32:/*メディアのイジェクトの禁止/許可を設定*/
	case 0x36:/*SASI 装置を初期化する*/
	case 0x37:/*SASI 装置をフォーマットする*/
	case 0x38:/*SASI 装置の破損トラックを使用不能にする*/
	case 0x39:/*SASI 装置を代替トラックを設定する*/
		m68000_set_reg(M68K_D0,0);/*OK(ダミー)*/
	 break;
	default:
	 break;
 }
  return;
}

/* 代替SCSI-IPL */
void
SCSI_ipl(void)
{
 uint32_t i,j;


	p6logd("SCSI-IPL\n");
	Memory_WriteD(0x7d4, 0xea004a);	// set SCSI-IOCS vect.
	uint32_t own_id = Memory_ReadB(0xed0070) & 0x07;

	SCSI_Device = 0;
	SCSI_Blocks = 0;
	SCSI_BlockSize = 1024;
	if(SCSI_BlockRead()==TRUE){
		for(j=0; j<SCSI_BlockSize; j++){
		  Memory_WriteB(0x02000+j,SCSI_Buf[j]);
		}
		if((Memory_ReadD(0x02000) != 0x58363853)||(Memory_ReadD(0x02004) != 0x43534931)){ /*check X68SCSI1*/
		 printf("0");return;
		}
	}
	else{
		return;
	}
	SCSI_Blocks = 1;
	if(SCSI_BlockRead()==TRUE){
		for(j=0; j<SCSI_BlockSize; j++){
		  Memory_WriteB(0x002000+j,SCSI_Buf[j]);
		}
	}
	else{
		return;
	}
	SCSI_Blocks = 2;
	if(SCSI_BlockRead()==TRUE){
		for(j=0; j<SCSI_BlockSize; j++){
		  Memory_WriteB(0x002400+j,SCSI_Buf[j]);
		}
	}
	else{
		return;
	}

	if(Memory_ReadB(0x02000) != 0x60)
		return;

	m68000_set_reg(M68K_A1, 0x002000);/*OK*/

  return;
}

/* 代替Human-IPL */
void
Human_ipl(void)
{
 uint32_t i,j;


	p6logd("Human-IPL\n");
	Memory_WriteD(0x7d4, 0xea004a);	// set SCSI-IOCS vect.
	uint32_t own_id = Memory_ReadB(0xed0070) & 0x07;

	//boot情報 自分は起動不可
	//IOCSベクタセット
	//３条件　２分岐
	//1.SCSI-0-7 状態chk-OKなら容量chk→パ-ティッション情報X68Kをchk SCHDISK-READ
	//相対アドレスになっているので、絶対アドレスへ
	//errはD2にff、OKなら情報入れる。


	SCSI_Device = 0;
	SCSI_Blocks = 0;
	SCSI_BlockSize = 1024;
	if(SCSI_BlockRead()==TRUE){
		for(j=0; j<SCSI_BlockSize; j++){
		  Memory_WriteB(0x02000+j,SCSI_Buf[j]);
		}
		if((Memory_ReadD(0x02000) != 0x58363853)||(Memory_ReadD(0x02004) != 0x43534931)){ /*check X68SCSI1*/
		 printf("0");return;
		}
	}

	SCSI_Blocks = 2;
	if(SCSI_BlockRead()==TRUE){
		for(j=0; j<SCSI_BlockSize; j++){
		  Memory_WriteB(0x02000+j,SCSI_Buf[j]);
		}
		if(Memory_ReadD(0x2000)!=0x5836384B){ /*check X68K*/
		 printf("3");return;
		}
		if((Memory_ReadD(0x02010) != 0x48756D61)||(Memory_ReadD(0x02014) != 0x6E36386B)){ /*check Humen68k*/
		 printf("4");return;
		}
	}
	else{
		return;
	}


  return;
}


// -----------------------------------
//   Read (Ex-SCSI area)
// -----------------------------------
uint8_t FASTCALL SCSI_Read(uint32_t adr)
{

/*MB89352 SCSI Protocol Controller (SPC) Register set*/
uint8_t ret = 0xff;

uint32_t j;

 switch(adr)
 {
	case 0xea0000:
	case 0xea0002:
	case 0xea0004:
	case 0xea0006:
	case 0xea0008:
	case 0xea000a:
	case 0xea000c:
	case 0xea000e:
	case 0xea0010:
	case 0xea0012:
	case 0xea0014:
	case 0xea0016:
	case 0xea0018:
	case 0xea001a:
	case 0xea001c:
	case 0xea001e:
	  break;
	case 0xea0001:
	  ret = 0x01 << scsi_bdid;
	  break;
	case 0xea0003:
	  ret = scsi_sctl ;
	  break;
	case 0xea0005:
	  ret = scsi_scmd ;
	  break;
	case 0xea0007:/*non fanc.*/
	  return 0;
	case 0xea0009:
	  ret = scsi_ints ;
	  break;
	case 0xea000b:
	  scsi_psns = SetPSNS();
	  ret = scsi_psns ;
	  break;
	case 0xea000d:
	  scsi_ssts = SetSSTS();
	  ret = scsi_ssts ;
	  break;
	case 0xea000f:
	  ret = scsi_serr ;
	  break;
	case 0xea0011:
	  ret = scsi_pctl ;
	  break;
	case 0xea0013:
	  ret = scsi_mbc ;
	  break;
	case 0xea0015:
	  ret = scsi_dreg ;
	  break;
	case 0xea0017:
	  ret = scsi_temp ;
	  break;
	case 0xea0019:
	  ret = ((scsi_TrCnt >> 16) & 0xff);
	  break;
	case 0xea001b:
	  ret = ((scsi_TrCnt >>  8) & 0xff);
	  break;
	case 0xea001d:
	  ret = ((scsi_TrCnt      ) & 0xff);
	  break;
	case 0xea001f:
	  break;
	default:
#ifndef __BIG_ENDIAN__
	  ret = SCSIIPL[(adr^1)&0x1fff];/*SCSI-IPL_ROM*/
#else
	  ret = SCSIIPL[(adr  )&0x1fff];/*SCSI-IPL_ROM*/
#endif
	  break;
 }

	return ret;

}


// -----------------------------------------------
//   Write (Ex-SCSI area)
// -----------------------------------------------
void FASTCALL SCSI_Write(uint32_t adr, uint8_t data)
{
	uint8_t i;

 switch(adr)
 {
	case 0xea0000:
	case 0xea0002:
	case 0xea0004:
	case 0xea0006:
	case 0xea0008:
	case 0xea000a:
	case 0xea000c:
	case 0xea000e:
	case 0xea0010:
	case 0xea0012:
	case 0xea0014:
	case 0xea0016:
	case 0xea0018:
	case 0xea001a:
	case 0xea001c:
	case 0xea001e:
	  break;
	case 0xea0001:
	  scsi_bdid = (data & 0x07);
	  break;
	case 0xea0003:
	  scsi_sctl = data;
	  if(scsi_sctl & 0x80) init_SPCreg();/*SPC init*/
	  if(scsi_sctl & 0x40) Bus_Reset();/*Transfer reset*/
	  break;
	case 0xea0005:
	  scsi_scmd = data;
	  if(scsi_scmd & 0x10){ scsi_rst = 1; } /*RST set*/
	  else{ scsi_rst = 0; } /*RST reset(通常動作)*/
	  switch(scsi_scmd & 0xe0) /*SPC Command*/
	  {
		case scmd_spc_busr:/*Bus free*/
		  scsi_phase = 0;
		  break;
		case scmd_spc_sel: /*select*/
		  i = scsi_temp & ~(0x01 << scsi_bdid);/*自分自身はマスク*/
		  if((scsi_sctl & sctl_arb_enbl) != 0){ /*アービトレーションする場合*/
			scsi_psns |= psns_scsi_bsy;
			scsi_ints |= ints_time_out;
			scsi_TrCnt = 0;
			scsi_len =  0; /*DREG Empty*/
			printf(" Arbitration \n");
			scsi_phase = 1;
		  }
		  //SCSI_targetselect(i);
		  scsi_phase = 2;
		  scsi_ints |= ints_cmd_comp;
		  scsi_psns |= psns_scsi_sel;
		  scsi_psns |= psns_scsi_bsy;
		  break;
		case scmd_spc_ratn: /* Reset ATN*/
		case scmd_spc_satn: /* Set ATN*/
		case scmd_spc_trns: /* Trasfer Data*/
		case scmd_spc_trnsp: /* Trasfer pause*/
		case scmd_spc_rstack: /* Reset ACK*/
		case scmd_spc_setack: /* Set ACK*/
		default:
		  break;
	  }
	  break;
	case 0xea0007:/*non fanc.*/
	  return;
	case 0xea0009: /* int reset */
	  scsi_ints &= ~data;
	  break;
	case 0xea000b: /*for Self chack*/
	  scsi_sdgc = data;
	  break;
	case 0xea000d:/*non fanc.*/
	  return;
	case 0xea000f:/*non fanc.*/
	  return;
	case 0xea0011:/*SCSI Transfer Control*/
	  scsi_pctl = data;
	  break;
	case 0xea0013:/*non fanc.*/
	  return;
	case 0xea0015:
	  scsi_dreg = data;
	  break;
	case 0xea0017:
	  scsi_temp = data;
	  break;
	case 0xea0019:
	  scsi_TrCnt = ((scsi_TrCnt &  0x00ffff) | (data << 16));
	  break;
	case 0xea001b:
	  scsi_TrCnt = ((scsi_TrCnt &  0xff00ff) | (data <<  8));
	  break;
	case 0xea001d:
	  scsi_TrCnt = ((scsi_TrCnt &  0xffff00) | (data      ));
	  break;
	case 0xea001f:
	  break;

	case 0xea0100:
	  SCSI_iocs(data);// SCSI-IOCS
	  break;
	case 0xea0200:
	  SCSI_ipl();	// SCSI 起動
	  break;
	case 0xea0300:
	  Human_ipl();	// Human-IOCS
	  break;

	default:
	  BusErrFlag = 1;
	  break;
 }

return;
}


/* Target DISK selection */
void SCSI_targetselect(uint8_t id)
{
	switch(id)
	{
	case 0x01: SCSI_Device = 0; //HDD01
		break;
	case 0x02: SCSI_Device = 1; //HDD02
		break;
	case 0x04: SCSI_Device = 2;
		break;
	case 0x08: SCSI_Device = 3;
		break;
	case 0x10: SCSI_Device = 4;
		break;
	case 0x20: SCSI_Device = 5; //MO
		break;
	case 0x40: SCSI_Device = 6; //CD-ROM
		break;
	case 0x80: SCSI_Device = 7; //X68000
		break;
	}

return;
}

/* Set bit PSNS register */
uint8_t SetPSNS()
{
  uint8_t psns = 0;

	/* bit0:I/O */
	if (scsi_io ) psns |= 0x01;
	/* bit1:C/D */
	if (scsi_cd ) psns |= 0x02;
	/* bit2:MSG */
	if (scsi_msg) psns |= 0x04;
	/* bit3:BSY */
	if (scsi_bsy) psns |= 0x08;
	/* bit4:SEL*/
	if (scsi_sel) psns |= 0x10;
	/* bit5:ATN */
	if (scsi_atn) psns |= 0x20;
	/* bit6:ACK */
	if (scsi_ack) psns |= 0x40;
	/* bit7:REQ */
	if (scsi_req) psns |= 0x80;

  return psns;
}


/* Set bit SSTS register */
uint8_t SetSSTS(){

 uint8_t ssts=0;

 /* bit 0&1 FIFO condition */
 if(scsi_trans != 0){
   if (scsi_len == 0) ssts |= 0x01; /* empty */
   else               ssts |= 0x02; /* not empty */
 }
 /* bit 2 TC=0 ? */
 if(scsi_TrCnt == 0) ssts |= 0x04;
 /* bit 3 RESET ? */
 if(scsi_rst != 0)  ssts |= 0x08;
 /* bit 4 Transfer Progress */
 if(scsi_trans != 0) ssts |= 0x10;
 /* bit 5 TC=0? */
 if(scsi_bsy != 0)  ssts |= 0x20;
 /* bit 6&7 (Initiate Mode) */
 if(scsi_phase != 0) ssts |= 0x80; /* BusFree?*/

 return ssts;
}

// ----------------------
//   Get File Size
// ----------------------
uint32_t Get_FileSize(uint8_t SCSI_ID)
{
	FILEH fp;

	fp = File_Open((char *)(char *)Config.SCSIEXHDImage[SCSI_ID]);
	if (!fp){
		File_Close(fp);
		return FALSE;
	}

	uint32_t size = File_Seek(fp, SCSI_Blocks*SCSI_BlockSize, FSEEK_END);
	if (size == 0xffffffff){//Error
		File_Close(fp);
		return FALSE;
	}

	File_Close(fp);
	return size;

}

// -----------------------------------------------------------------------
//   SCSI BlockRead （SCSI_BlocksからBlockSize分ReadしてSCSI_Bufに格納）
// -----------------------------------------------------------------------
BOOL SCSI_BlockRead(void)
{
	FILEH fp;
	memset(SCSI_Buf, 0, SCSI_BlockSize);
	fp = File_Open((char *)Config.SCSIEXHDImage[SCSI_Device]);
	if (!fp)
	{
		memset(SCSI_Buf, 0, SCSI_BlockSize);
		return FALSE;
	}
	if (File_Seek(fp, SCSI_Blocks*SCSI_BlockSize, FSEEK_SET)!=(SCSI_Blocks*SCSI_BlockSize)) 
	{
		File_Close(fp);
		return FALSE;
	}
	if (File_Read(fp, SCSI_Buf, SCSI_BlockSize)!=SCSI_BlockSize)
	{
		File_Close(fp);
		return FALSE;
	}
	File_Close(fp);

	return TRUE;/*success*/
}


// -----------------------------------------------------------------------
//   SCSI BlockWrite（SCSI_BlocksからBlockSize分,Write）
// -----------------------------------------------------------------------
BOOL SCSI_BlockWrite(void)
{	FILEH fp;

	fp = File_Open((char *)Config.SCSIEXHDImage[SCSI_Device]);
	if (!fp) return FALSE;
	if (File_Seek(fp, SCSI_Blocks*SCSI_BlockSize, FSEEK_SET)!=(SCSI_Blocks*SCSI_BlockSize))
	{
		File_Close(fp);
		return FALSE;
	}
	if (File_Write(fp, SCSI_Buf, SCSI_BlockSize)!=SCSI_BlockSize)
	{
		File_Close(fp);
		return FALSE;
	}
	File_Close(fp);

	return TRUE;/*success*/
}

