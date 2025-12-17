// -----------------------------------------
// irq.c for Exception Handler
//
// Exception Priority
// High 7 NMI
//      6 MFP 
//      5 SCC MIDI
//      4 Exp.(Merqury)
//      3 DMAC
//      2 Exp.
// Low  1 FDC SASI PRT
// -----------------------------------------

#include "../m68000/m68000.h"
#include "irqh.h"


uint8_t  IRQH_IRQ[8];
void  *IRQH_CallBack[8];

// -----------------------------------------
//   初期化
// -----------------------------------------
void IRQH_Init(void)
{
	memset(IRQH_IRQ, 0, sizeof(IRQH_IRQ));
}


// -----------------------------------------
//   デフォルトのベクタを返す（これが起こったら変だお）
// -----------------------------------------
uint32_t IRQH_DefaultVector(uint8_t irq)
{
	IRQH_IRQCallBack(irq);
	return 0xffffffff;
}


// -----------------------------------------
//   他の割り込みのチェック
//   各デバイスのベクタを返すルーチンから呼ばれます
// -----------------------------------------
void IRQH_IRQCallBack(uint8_t irq)
{
	IRQH_IRQ[irq&7] = 0;

	m68000_set_irq_line(0);

	for (int i=7; i>0; i--) // Hight to Low Priority
	{
	    if (IRQH_IRQ[i])
	    {
			m68000_set_irq_line(i);
			return;
	    }
	}
}

// -----------------------------------------
//   割り込み発生
// -----------------------------------------
void IRQH_Int(uint8_t irq, void* handler)
{
	IRQH_IRQ[irq&7] = 1;

	if (handler==NULL)
	    IRQH_CallBack[irq&7] = &IRQH_DefaultVector;
	else
	    IRQH_CallBack[irq&7] = handler;

	for (int i=7; i>0; i--)
	{
	    if (IRQH_IRQ[i])
	    {
	        m68000_set_irq_line(i);
	        return;
	    }
	}

}

// -----------------------------------------
//   from MC68000
// -----------------------------------------
int32_t  my_irqh_callback(int32_t  level)
{
	typedef int32_t  DMMY_INT_CALLBACK(int32_t level);//dummy 関数

    DMMY_INT_CALLBACK *func = IRQH_CallBack[level&7];
    int32_t vect = (func)(level&7);

    for (int i=7; i>0; i--)
    {
		if (IRQH_IRQ[i])
		{
	    	m68000_set_irq_line(i);
			break;
		}
    }

    return (int32_t)vect;
}

