// YM2612 FM sound chip emulator interface
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
// Copyright 2001 Jarek Burczynski
// Copyright 1998 Tatsuyuki Satoh
// Copyright 1997 Nicola Salmoria and the MAME team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// derived from: Game_Music_Emu 0.5.2
// Version 1.4 (final beta)
//

#include "yamaha_ym2612.hpp"
#include <cstdint>
#include <math.h>
#include <cstdlib>
#include <cstring>

extern signed int tl_tab[];
extern unsigned int sin_tab[];
extern const uint32_t sl_table[];
extern const uint8_t eg_inc[];
extern const uint8_t eg_rate_select[];
extern const uint8_t eg_rate_shift[];
extern const uint8_t dt_tab[];
extern const uint8_t opn_fktable[];
extern const uint32_t lfo_samples_per_step[];
extern const uint8_t lfo_ams_depth_shift[];
extern const uint8_t lfo_pm_output[7 * 8][8];
extern int32_t lfo_pm_table[];

/* status set and IRQ handling */
static inline void FM_STATUS_SET(FM_ST *ST,int flag)
{
    /* set status flag */
    ST->status |= flag;
    if ( !(ST->irq) && (ST->status & ST->irqmask) )
    {
        ST->irq = 1;
        /* callback user interrupt handler (IRQ is OFF to ON) */
        //if (ST->IRQ_Handler) (ST->IRQ_Handler)(ST->device,1);
    }
}

/* status reset and IRQ handling */
static inline void FM_STATUS_RESET(FM_ST *ST,int flag)
{
    /* reset status flag */
    ST->status &=~flag;
    if ( (ST->irq) && !(ST->status & ST->irqmask) )
    {
        ST->irq = 0;
        /* callback user interrupt handler (IRQ is ON to OFF) */
        //if (ST->IRQ_Handler) (ST->IRQ_Handler)(ST->device,0);
    }
}

/* IRQ mask set */
static inline void FM_IRQMASK_SET(FM_ST *ST,int flag)
{
    ST->irqmask = flag;
    /* IRQ handling check */
    FM_STATUS_SET(ST,0);
    FM_STATUS_RESET(ST,0);
}

/* OPN Mode Register Write */
static inline void set_timers( FM_ST *ST, int v )
{
    /* b7 = CSM MODE */
    /* b6 = 3 slot mode */
    /* b5 = reset b */
    /* b4 = reset a */
    /* b3 = timer enable b */
    /* b2 = timer enable a */
    /* b1 = load b */
    /* b0 = load a */
    ST->mode = v;

    /* reset Timer b flag */
    if ( v & 0x20 )
        FM_STATUS_RESET(ST,0x02);
    /* reset Timer a flag */
    if ( v & 0x10 )
        FM_STATUS_RESET(ST,0x01);
    /* load b */
    if ( v & 0x02 )
    {
        if ( ST->TBC == 0 )
        {
            ST->TBC = ( 256-ST->TB)<<4;
        }
    }
    else
    {   /* stop timer b */
        if ( ST->TBC != 0 )
        {
            ST->TBC = 0;
        }
    }
    /* load a */
    if ( v & 0x01 )
    {
        if ( ST->TAC == 0 )
        {
            ST->TAC = (1024-ST->TA);
        }
    }
    else
    {   /* stop timer a */
        if ( ST->TAC != 0 )
        {
            ST->TAC = 0;
        }
    }
}


/* Timer A Overflow */
static inline void TimerAOver(FM_ST *ST)
{
    /* set status (if enabled) */
    if (ST->mode & 0x04) FM_STATUS_SET(ST,0x01);
    /* clear or reload the counter */
    ST->TAC = (1024-ST->TA);
}
/* Timer B Overflow */
static inline void TimerBOver(FM_ST *ST)
{
    /* set status (if enabled) */
    if (ST->mode & 0x08) FM_STATUS_SET(ST,0x02);
    /* clear or reload the counter */
    ST->TBC = ( 256-ST->TB)<<4;
}


static inline void FM_KEYON(FM_CH *CH , int s )
{
    FM_SLOT *SLOT = &CH->SLOT[s];
    if ( !SLOT->key )
    {
        SLOT->key = 1;
        SLOT->phase = 0;        /* restart Phase Generator */
        SLOT->ssgn = (SLOT->ssg & 0x04) >> 1;
        SLOT->state = EG_ATT;
    }
}

static inline void FM_KEYOFF(FM_CH *CH , int s )
{
    FM_SLOT *SLOT = &CH->SLOT[s];
    if ( SLOT->key )
    {
        SLOT->key = 0;
        if (SLOT->state>EG_REL)
            SLOT->state = EG_REL;/* phase -> Release */
    }
}

/* set algorithm connection */
static void setup_connection( FM_OPN *OPN, FM_CH *CH, int ch )
{
    int32_t *carrier = &OPN->out_fm[ch];

    int32_t **om1 = &CH->connect1;
    int32_t **om2 = &CH->connect3;
    int32_t **oc1 = &CH->connect2;

    int32_t **memc = &CH->mem_connect;

    switch( CH->ALGO )
    {
    case 0:
        /* M1---C1---MEM---M2---C2---OUT */
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 1:
        /* M1------+-MEM---M2---C2---OUT */
        /*      C1-+                     */
        *om1 = &OPN->mem;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 2:
        /* M1-----------------+-C2---OUT */
        /*      C1---MEM---M2-+          */
        *om1 = &OPN->c2;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->m2;
        break;
    case 3:
        /* M1---C1---MEM------+-C2---OUT */
        /*                 M2-+          */
        *om1 = &OPN->c1;
        *oc1 = &OPN->mem;
        *om2 = &OPN->c2;
        *memc= &OPN->c2;
        break;
    case 4:
        /* M1---C1-+-OUT */
        /* M2---C2-+     */
        /* MEM: not used */
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = &OPN->c2;
        *memc= &OPN->mem;   /* store it anywhere where it will not be used */
        break;
    case 5:
        /*    +----C1----+     */
        /* M1-+-MEM---M2-+-OUT */
        /*    +----C2----+     */
        *om1 = nullptr;   /* special mark */
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->m2;
        break;
    case 6:
        /* M1---C1-+     */
        /*      M2-+-OUT */
        /*      C2-+     */
        /* MEM: not used */
        *om1 = &OPN->c1;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;   /* store it anywhere where it will not be used */
        break;
    case 7:
        /* M1-+     */
        /* C1-+-OUT */
        /* M2-+     */
        /* C2-+     */
        /* MEM: not used*/
        *om1 = carrier;
        *oc1 = carrier;
        *om2 = carrier;
        *memc= &OPN->mem;   /* store it anywhere where it will not be used */
        break;
    }

    CH->connect4 = carrier;
}

/* set detune & multiple */
static inline void set_det_mul(FM_ST *ST,FM_CH *CH,FM_SLOT *SLOT,int v)
{
    SLOT->mul = (v&0x0f)? (v&0x0f)*2 : 1;
    SLOT->DT  = ST->dt_tab[(v>>4)&7];
    CH->SLOT[SLOT1].Incr=-1;
}

/// Set total level.
///
/// @param CH a pointer to the channel
/// @param FM_SLOT a pointer to the operator
/// @param v the value for the TL register
///
static inline void set_tl(FM_CH *CH, FM_SLOT *SLOT, int v) {
    // the TL is 7 bits
    SLOT->tl = (v & 0x7f) << (ENV_BITS - 7);
}

/* set attack rate & key scale  */
static inline void set_ar_ksr(FM_CH *CH,FM_SLOT *SLOT,int v) {
    uint8_t old_KSR = SLOT->KSR;

    SLOT->ar = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

    SLOT->KSR = 3-(v>>6);
    if (SLOT->KSR != old_KSR)
    {
        CH->SLOT[SLOT1].Incr=-1;
    }

    /* refresh Attack rate */
    if ((SLOT->ar + SLOT->ksr) < 32+62)
    {
        SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
        SLOT->eg_sel_ar = eg_rate_select[SLOT->ar  + SLOT->ksr ];
    }
    else
    {
        SLOT->eg_sh_ar  = 0;
        SLOT->eg_sel_ar = 17*RATE_STEPS;
    }
}

/* set decay rate */
static inline void set_dr(FM_SLOT *SLOT,int v) {
    SLOT->d1r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

    SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
    SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
}

/* set sustain rate */
static inline void set_sr(FM_SLOT *SLOT,int v) {
    SLOT->d2r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

    SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
    SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
}

/* set release rate */
static inline void set_sl_rr(FM_SLOT *SLOT,int v) {
    SLOT->sl = sl_table[ v>>4 ];

    SLOT->rr  = 34 + ((v&0x0f)<<2);

    SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];
    SLOT->eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];
}

static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p;

    p = (env<<3) + sin_tab[ ( ((signed int)((phase & ~FREQ_MASK) + (pm<<15))) >> FREQ_SH ) & SIN_MASK ];

    if (p >= TL_TAB_LEN)
        return 0;
    return tl_tab[p];
}

static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm) {
    uint32_t p;

    p = (env<<3) + sin_tab[ ( ((signed int)((phase & ~FREQ_MASK) + pm      )) >> FREQ_SH ) & SIN_MASK ];

    if (p >= TL_TAB_LEN)
        return 0;
    return tl_tab[p];
}

/* advance LFO to next sample */
static inline void advance_lfo(FM_OPN *OPN) {
    if (OPN->lfo_timer_overflow) {  /* LFO enabled ? */
        /* increment LFO timer */
        OPN->lfo_timer +=  OPN->lfo_timer_add;

        /* when LFO is enabled, one level will last for 108, 77, 71, 67, 62, 44, 8 or 5 samples */
        while (OPN->lfo_timer >= OPN->lfo_timer_overflow) {
            OPN->lfo_timer -= OPN->lfo_timer_overflow;

            /* There are 128 LFO steps */
            OPN->lfo_cnt = ( OPN->lfo_cnt + 1 ) & 127;

            /* triangle (inverted) */
            /* AM: from 126 to 0 step -2, 0 to 126 step +2 */
            if (OPN->lfo_cnt<64)
                OPN->LFO_AM = (OPN->lfo_cnt ^ 63) << 1;
            else
                OPN->LFO_AM = (OPN->lfo_cnt & 63) << 1;

            /* PM works with 4 times slower clock */
            OPN->LFO_PM = OPN->lfo_cnt >> 2;
        }
    }
}

static inline void advance_eg_channel(FM_OPN *OPN, FM_SLOT *SLOT) {
    // four operators per channel
    unsigned int i = 4;
    do {  // reset SSG-EG swap flag
        unsigned int swap_flag = 0;
        switch(SLOT->state) {
        case EG_ATT:  // attack phase
            if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_ar) - 1))) {
                SLOT->volume += (~SLOT->volume * (eg_inc[SLOT->eg_sel_ar + ((OPN->eg_cnt>>SLOT->eg_sh_ar) & 7)])) >> 4;
                if (SLOT->volume <= MIN_ATT_INDEX) {
                    SLOT->volume = MIN_ATT_INDEX;
                    SLOT->state = EG_DEC;
                }
            }
            break;
        case EG_DEC:  // decay phase
            if (SLOT->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d1r) - 1))) {
                    SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r) & 7)];
                    if ( SLOT->volume >= static_cast<int32_t>(SLOT->sl) )
                        SLOT->state = EG_SUS;
                }
            } else {
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d1r) - 1))) {
                    SLOT->volume += eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r) & 7)];
                    if (SLOT->volume >= static_cast<int32_t>(SLOT->sl))
                        SLOT->state = EG_SUS;
                }
            }
            break;
        case EG_SUS:  // sustain phase
            if (SLOT->ssg & 0x08) {  // SSG EG type envelope selected
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d2r) - 1))) {
                    SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r) & 7)];
                    if (SLOT->volume >= ENV_QUIET) {
                        SLOT->volume = MAX_ATT_INDEX;
                        if (SLOT->ssg & 0x01) {  // bit 0 = hold
                            if (SLOT->ssgn & 1) {  // have we swapped once ???
                                // yes, so do nothing, just hold current level
                            } else {  // bit 1 = alternate
                                swap_flag = (SLOT->ssg & 0x02) | 1;
                            }
                        } else {  // same as KEY-ON operation
                            // restart of the Phase Generator should be here
                            SLOT->phase = 0;
                            // phase -> Attack
                            SLOT->volume = 511;
                            SLOT->state = EG_ATT;
                            // bit 1 = alternate
                            swap_flag = (SLOT->ssg & 0x02);
                        }
                    }
                }
            } else {
                if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_d2r) - 1))) {
                    SLOT->volume += eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r) & 7)];
                    if (SLOT->volume >= MAX_ATT_INDEX) {
                        SLOT->volume = MAX_ATT_INDEX;
                        // do not change SLOT->state (verified on real chip)
                    }
                }
            }
            break;
        case EG_REL:  // release phase
            if (!(OPN->eg_cnt & ((1 << SLOT->eg_sh_rr) - 1))) {
                // SSG-EG affects Release phase also (Nemesis)
                SLOT->volume += eg_inc[SLOT->eg_sel_rr + ((OPN->eg_cnt>>SLOT->eg_sh_rr) & 7)];
                if (SLOT->volume >= MAX_ATT_INDEX) {
                    SLOT->volume = MAX_ATT_INDEX;
                    SLOT->state = EG_OFF;
                }
            }
            break;
        }
        // get the output volume from the slot
        unsigned int out = static_cast<uint32_t>(SLOT->volume);
        // negate output (changes come from alternate bit, init comes from
        // attack bit)
        if ((SLOT->ssg & 0x08) && (SLOT->ssgn & 2) && (SLOT->state > EG_REL))
            out ^= MAX_ATT_INDEX;
        // we need to store the result here because we are going to change
        // ssgn in next instruction
        SLOT->vol_out = out + SLOT->tl;
        // reverse SLOT inversion flag
        SLOT->ssgn ^= swap_flag;
        // increment the slot and decrement the iterator
        SLOT++;
        i--;
    } while (i);
}

#define volume_calc(OP) ((OP)->vol_out + (AM & (OP)->AMmask))

// static inline void update_phase_lfo_slot(FM_OPN *OPN, FM_SLOT *SLOT, int32_t pms, uint32_t block_fnum)
// {
//     uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
//     int32_t  lfo_fn_table_index_offset = lfo_pm_table[ fnum_lfo + pms + OPN->LFO_PM ];

//     if (lfo_fn_table_index_offset)    /* LFO phase modulation active */
//     {
//         uint8_t blk;
//         uint32_t fn;
//         int kc, fc;

//         block_fnum = block_fnum*2 + lfo_fn_table_index_offset;

//         blk = (block_fnum&0x7000) >> 12;
//         fn  = block_fnum & 0xfff;

//         /* keyscale code */
//         kc = (blk<<2) | opn_fktable[fn >> 8];

//         /* phase increment counter */
//         fc = (OPN->fn_table[fn]>>(7-blk)) + SLOT->DT[kc];

//         /* detects frequency overflow (credits to Nemesis) */
//         if (fc < 0) fc += OPN->fn_max;

//         /* update phase */
//         SLOT->phase += (fc * SLOT->mul) >> 1;
//     }
//     else    /* LFO phase modulation  = zero */
//     {
//         SLOT->phase += SLOT->Incr;
//     }
// }

static inline void update_phase_lfo_channel(FM_OPN *OPN, FM_CH *CH)
{
    uint32_t block_fnum = CH->block_fnum;

    uint32_t fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
    int32_t  lfo_fn_table_index_offset = lfo_pm_table[ fnum_lfo + CH->pms + OPN->LFO_PM ];

    if (lfo_fn_table_index_offset)    /* LFO phase modulation active */
    {
            uint8_t blk;
            uint32_t fn;
        int kc, fc, finc;

        block_fnum = block_fnum*2 + lfo_fn_table_index_offset;

            blk = (block_fnum&0x7000) >> 12;
            fn  = block_fnum & 0xfff;

        /* keyscale code */
            kc = (blk<<2) | opn_fktable[fn >> 8];

            /* phase increment counter */
        fc = (OPN->fn_table[fn]>>(7-blk));

        /* detects frequency overflow (credits to Nemesis) */
        finc = fc + CH->SLOT[SLOT1].DT[kc];

        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT1].phase += (finc*CH->SLOT[SLOT1].mul) >> 1;

        finc = fc + CH->SLOT[SLOT2].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT2].phase += (finc*CH->SLOT[SLOT2].mul) >> 1;

        finc = fc + CH->SLOT[SLOT3].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT3].phase += (finc*CH->SLOT[SLOT3].mul) >> 1;

        finc = fc + CH->SLOT[SLOT4].DT[kc];
        if (finc < 0) finc += OPN->fn_max;
        CH->SLOT[SLOT4].phase += (finc*CH->SLOT[SLOT4].mul) >> 1;
    }
    else    /* LFO phase modulation  = zero */
    {
            CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
            CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
            CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
            CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
    }
}

static inline void chan_calc(FM_OPN *OPN, FM_CH *CH)
{
    unsigned int eg_out;

    uint32_t AM = OPN->LFO_AM >> CH->ams;


    OPN->m2 = OPN->c1 = OPN->c2 = OPN->mem = 0;

    *CH->mem_connect = CH->mem_value;   /* restore delayed sample (MEM) value to m2 or c2 */

    eg_out = volume_calc(&CH->SLOT[SLOT1]);
    {
        int32_t out = CH->op1_out[0] + CH->op1_out[1];
        CH->op1_out[0] = CH->op1_out[1];

        if ( !CH->connect1 )
        {
            /* algorithm 5  */
            OPN->mem = OPN->c1 = OPN->c2 = CH->op1_out[0];
        }
        else
        {
            /* other algorithms */
            *CH->connect1 += CH->op1_out[0];
        }

        CH->op1_out[1] = 0;
        if ( eg_out < ENV_QUIET )    /* SLOT 1 */
        {
            if (!CH->FB)
                out=0;

            CH->op1_out[1] = op_calc1(CH->SLOT[SLOT1].phase, eg_out, (out<<CH->FB) );
        }
    }



    eg_out = volume_calc(&CH->SLOT[SLOT3]);
    if ( eg_out < ENV_QUIET )        /* SLOT 3 */
        *CH->connect3 += op_calc(CH->SLOT[SLOT3].phase, eg_out, OPN->m2);


    eg_out = volume_calc(&CH->SLOT[SLOT2]);
    if ( eg_out < ENV_QUIET )        /* SLOT 2 */
        *CH->connect2 += op_calc(CH->SLOT[SLOT2].phase, eg_out, OPN->c1);

    eg_out = volume_calc(&CH->SLOT[SLOT4]);
    if ( eg_out < ENV_QUIET )        /* SLOT 4 */
        *CH->connect4 += op_calc(CH->SLOT[SLOT4].phase, eg_out, OPN->c2);


    /* store current MEM */
    CH->mem_value = OPN->mem;


    /* update phase counters AFTER output calculations */
    if (CH->pms)
    {
        update_phase_lfo_channel(OPN, CH);
    }
    else    /* no LFO phase modulation */
    {
        CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
        CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
        CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
        CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
    }


}

/* update phase increment and envelope generator */
static inline void refresh_fc_eg_slot(FM_OPN *OPN, FM_SLOT *SLOT , int fc , int kc )
{
    int ksr = kc >> SLOT->KSR;


    fc += SLOT->DT[kc];

    /* detects frequency overflow (credits to Nemesis) */
    if (fc < 0) fc += OPN->fn_max;

    /* (frequency) phase increment counter */
    SLOT->Incr = (fc * SLOT->mul) >> 1;

    if ( SLOT->ksr != ksr )
    {
        SLOT->ksr = ksr;

        /* calculate envelope generator rates */
        if ((SLOT->ar + SLOT->ksr) < 32+62)
        {
            SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
            SLOT->eg_sel_ar = eg_rate_select[SLOT->ar  + SLOT->ksr ];
        }
        else
        {
            SLOT->eg_sh_ar  = 0;
            SLOT->eg_sel_ar = 17*RATE_STEPS;
        }

        SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
        SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
        SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];

        SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
        SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
        SLOT->eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];
    }
}


/* update phase increment counters */
/* Changed from static inline to static to work around gcc 4.2.1 codegen bug */

static void refresh_fc_eg_chan(FM_OPN *OPN, FM_CH *CH )
{
    if ( CH->SLOT[SLOT1].Incr==-1)
    {
        int fc = CH->fc;
        int kc = CH->kcode;
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT1] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT2] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT3] , fc , kc );
        refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT4] , fc , kc );
    }
}

/*

// initialize time tables
static void init_timetables( FM_ST *ST , const uint8_t *dttable )
{
    int i,d;
    double rate;


    // DeTune table
    for (d = 0;d <= 3;d++)
    {
        for (i = 0;i <= 31;i++)
        {
            rate = ((double)dttable[d*32 + i]) * SIN_LEN  * ST->freqbase  * (1<<FREQ_SH) / ((double)(1<<20));
            ST->dt_tab[d][i]   = (int32_t) rate;
            ST->dt_tab[d+4][i] = -ST->dt_tab[d][i];
        }
    }

}
*/

static void reset_channels( FM_ST *ST , FM_CH *CH , int num )
{
    int c,s;

    ST->mode   = 0; /* normal mode */
    ST->TA     = 0;
    ST->TAC    = 0;
    ST->TB     = 0;
    ST->TBC    = 0;

    for( c = 0 ; c < num ; c++ )
    {
        CH[c].fc = 0;
        for(s = 0 ; s < 4 ; s++ )
        {
            CH[c].SLOT[s].ssg = 0;
            CH[c].SLOT[s].ssgn = 0;
            CH[c].SLOT[s].state= EG_OFF;
            CH[c].SLOT[s].volume = MAX_ATT_INDEX;
            CH[c].SLOT[s].vol_out= MAX_ATT_INDEX;
        }
    }
}



/* SSG-EG update process */
/* The behavior is based upon Nemesis tests on real hardware */
/* This is actually executed before each samples */
static void update_ssg_eg_channel(FM_SLOT *SLOT)
{
    unsigned int i = 4; /* four operators per channel */

    do
    {
        /* detect SSG-EG transition */
        /* this is not required during release phase as the attenuation has been forced to MAX and output invert flag is not used */
        /* if an Attack Phase is programmed, inversion can occur on each sample */
        if ((SLOT->ssg & 0x08) && (SLOT->volume >= 0x200) && (SLOT->state > EG_REL))
        {
            if (SLOT->ssg & 0x01)  /* bit 0 = hold SSG-EG */
            {
                /* set inversion flag */
                    if (SLOT->ssg & 0x02)
                        SLOT->ssgn = 4;

                /* force attenuation level during decay phases */
                if ((SLOT->state != EG_ATT) && !(SLOT->ssgn ^ (SLOT->ssg & 0x04)))
                    SLOT->volume  = MAX_ATT_INDEX;
            }
            else  /* loop SSG-EG */
            {
                /* toggle output inversion flag or reset Phase Generator */
                    if (SLOT->ssg & 0x02)
                        SLOT->ssgn ^= 4;
                    else
                        SLOT->phase = 0;

                /* same as Key ON */
                if (SLOT->state != EG_ATT)
                {
                    if ((SLOT->ar + SLOT->ksr) < 94 /*32+62*/)
                    {
                        SLOT->state = (SLOT->volume <= MIN_ATT_INDEX) ? ((SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC) : EG_ATT;
                    }
                    else
                    {
                        /* Attack Rate is maximal: directly switch to Decay or Substain */
                        SLOT->volume = MIN_ATT_INDEX;
                        SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
                    }
                }
            }

            /* recalculate EG output */
            if (SLOT->ssgn ^ (SLOT->ssg&0x04))
                SLOT->vol_out = ((uint32_t)(0x200 - SLOT->volume) & MAX_ATT_INDEX) + SLOT->tl;
            else
                SLOT->vol_out = (uint32_t)SLOT->volume + SLOT->tl;
        }


        /* next slot */
        SLOT++;
        i--;
    } while (i);
}







/* write a OPN mode register 0x20-0x2f */
static void OPNWriteMode(FM_OPN *OPN, int r, int v)
{
    uint8_t c;
    FM_CH *CH;



    switch(r)
    {
    case 0x21:  /* Test */
        break;
    case 0x22:  /* LFO FREQ (YM2608/YM2610/YM2610B/YM2612) */
        if (v&8) /* LFO enabled ? */
        {
            OPN->lfo_timer_overflow = lfo_samples_per_step[v&7] << LFO_SH;
        }
        else
        {
            /* hold LFO waveform in reset state */
            OPN->lfo_timer_overflow = 0;
            OPN->lfo_timer = 0;
            OPN->lfo_cnt   = 0;
            OPN->LFO_PM    = 0;
            OPN->LFO_AM    = 126;
        }
        break;
    case 0x24:  /* timer A High 8*/
        OPN->ST.TA = (OPN->ST.TA & 0x03)|(((int)v)<<2);
        break;
    case 0x25:  /* timer A Low 2*/
        OPN->ST.TA = (OPN->ST.TA & 0x3fc)|(v&3);
        break;
    case 0x26:  /* timer B */
        OPN->ST.TB = v;
        break;
    case 0x27:  /* mode, timer control */
        set_timers(&(OPN->ST),v );
        break;
    case 0x28:  /* key on / off */
        c = v & 0x03;
        if ( c == 3 ) break;
        if ( (v&0x04) && (OPN->type & TYPE_6CH) ) c+=3;
        CH = OPN->P_CH;
        CH = &CH[c];
        if (v&0x10) FM_KEYON(CH,SLOT1); else FM_KEYOFF(CH,SLOT1);
        if (v&0x20) FM_KEYON(CH,SLOT2); else FM_KEYOFF(CH,SLOT2);
        if (v&0x40) FM_KEYON(CH,SLOT3); else FM_KEYOFF(CH,SLOT3);
        if (v&0x80) FM_KEYON(CH,SLOT4); else FM_KEYOFF(CH,SLOT4);
        break;
    }
}

/* write a OPN register (0x30-0xff) */
static void OPNWriteReg(FM_OPN *OPN, int r, int v)
{
    FM_CH *CH;
    FM_SLOT *SLOT;

    uint8_t c = OPN_CHAN(r);

    if (c == 3) return; /* 0xX3,0xX7,0xXB,0xXF */

    if (r >= 0x100) c+=3;

    CH = OPN->P_CH;
    CH = &CH[c];

    SLOT = &(CH->SLOT[OPN_SLOT(r)]);

    switch( r & 0xf0 ) {
    case 0x30:  /* DET , MUL */
        set_det_mul(&OPN->ST,CH,SLOT,v);
        break;

    case 0x40:  /* TL */
        set_tl(CH,SLOT,v);
        break;

    case 0x50:  /* KS, AR */
        set_ar_ksr(CH,SLOT,v);
        break;

    case 0x60:  /* bit7 = AM ENABLE, DR */
        set_dr(SLOT,v);

        if (OPN->type & TYPE_LFOPAN) /* YM2608/2610/2610B/2612 */
        {
            SLOT->AMmask = (v&0x80) ? ~0 : 0;
        }
        break;

    case 0x70:  /*     SR */
        set_sr(SLOT,v);
        break;

    case 0x80:  /* SL, RR */
        set_sl_rr(SLOT,v);
        break;

    case 0x90:  /* SSG-EG */
        SLOT->ssg  =  v&0x0f;

            /* recalculate EG output */
        if ((SLOT->ssg&0x08) && (SLOT->ssgn ^ (SLOT->ssg&0x04)) && (SLOT->state > EG_REL))
            SLOT->vol_out = ((uint32_t)(0x200 - SLOT->volume) & MAX_ATT_INDEX) + SLOT->tl;
        else
            SLOT->vol_out = (uint32_t)SLOT->volume + SLOT->tl;

        /* SSG-EG envelope shapes :

        E AtAlH
        1 0 0 0  \\\\

        1 0 0 1  \___

        1 0 1 0  \/\/
                  ___
        1 0 1 1  \

        1 1 0 0  ////
                  ___
        1 1 0 1  /

        1 1 1 0  /\/\

        1 1 1 1  /___


        E = SSG-EG enable


        The shapes are generated using Attack, Decay and Sustain phases.

        Each single character in the diagrams above represents this whole
        sequence:

        - when KEY-ON = 1, normal Attack phase is generated (*without* any
          difference when compared to normal mode),

        - later, when envelope level reaches minimum level (max volume),
          the EG switches to Decay phase (which works with bigger steps
          when compared to normal mode - see below),

        - later when envelope level passes the SL level,
          the EG swithes to Sustain phase (which works with bigger steps
          when compared to normal mode - see below),

        - finally when envelope level reaches maximum level (min volume),
          the EG switches to Attack phase again (depends on actual waveform).

        Important is that when switch to Attack phase occurs, the phase counter
        of that operator will be zeroed-out (as in normal KEY-ON) but not always.
        (I havent found the rule for that - perhaps only when the output level is low)

        The difference (when compared to normal Envelope Generator mode) is
        that the resolution in Decay and Sustain phases is 4 times lower;
        this results in only 256 steps instead of normal 1024.
        In other words:
        when SSG-EG is disabled, the step inside of the EG is one,
        when SSG-EG is enabled, the step is four (in Decay and Sustain phases).

        Times between the level changes are the same in both modes.


        Important:
        Decay 1 Level (so called SL) is compared to actual SSG-EG output, so
        it is the same in both SSG and no-SSG modes, with this exception:

        when the SSG-EG is enabled and is generating raising levels
        (when the EG output is inverted) the SL will be found at wrong level !!!
        For example, when SL=02:
            0 -6 = -6dB in non-inverted EG output
            96-6 = -90dB in inverted EG output
        Which means that EG compares its level to SL as usual, and that the
        output is simply inverted afterall.


        The Yamaha's manuals say that AR should be set to 0x1f (max speed).
        That is not necessary, but then EG will be generating Attack phase.

        */


        break;

    case 0xa0:
        switch( OPN_SLOT(r) )
        {
        case 0:     /* 0xa0-0xa2 : FNUM1 */
            {
                uint32_t fn = (((uint32_t)( (OPN->ST.fn_h)&7))<<8) + v;
                uint8_t blk = OPN->ST.fn_h>>3;
                /* keyscale code */
                CH->kcode = (blk<<2) | opn_fktable[(fn >> 7) & 0xf];
                /* phase increment counter */
                CH->fc = OPN->fn_table[fn*2]>>(7-blk);

                /* store fnum in clear form for LFO PM calculations */
                CH->block_fnum = (blk<<11) | fn;

                CH->SLOT[SLOT1].Incr=-1;
            }
            break;
        case 1:     /* 0xa4-0xa6 : FNUM2,BLK */
            OPN->ST.fn_h = v&0x3f;
            break;
        case 2:     /* 0xa8-0xaa : 3CH FNUM1 */
            if (r < 0x100)
            {
                uint32_t fn = (((uint32_t)(OPN->SL3.fn_h&7))<<8) + v;
                uint8_t blk = OPN->SL3.fn_h>>3;
                /* keyscale code */
                OPN->SL3.kcode[c]= (blk<<2) | opn_fktable[(fn >> 7) & 0xf];
                /* phase increment counter */
                OPN->SL3.fc[c] = OPN->fn_table[fn*2]>>(7-blk);
                OPN->SL3.block_fnum[c] = (blk<<11) | fn;
                (OPN->P_CH)[2].SLOT[SLOT1].Incr=-1;
            }
            break;
        case 3:     /* 0xac-0xae : 3CH FNUM2,BLK */
            if (r < 0x100)
                OPN->SL3.fn_h = v&0x3f;
            break;
        }
        break;

    case 0xb0:

        switch( OPN_SLOT(r) )
        {
        case 0:     /* 0xb0-0xb2 : FB,ALGO */
            {
                int feedback = (v>>3)&7;
                CH->ALGO = v&7;
                CH->FB   = feedback ? feedback+6 : 0;
                setup_connection( OPN, CH, c );

            }
            break;
        case 1:     /* 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2610B/YM2610/YM2608) */
            if ( OPN->type & TYPE_LFOPAN)
            {
                /* b0-2 PMS */
                CH->pms = (v & 7) * 32; /* CH->pms = PM depth * 32 (index in lfo_pm_table) */

                /* b4-5 AMS */
                CH->ams = lfo_ams_depth_shift[(v>>4) & 0x03];


                /* PAN :  b7 = L, b6 = R */
                OPN->pan[ c*2   ] = (v & 0x80) ? ~0 : 0;
                OPN->pan[ c*2+1 ] = (v & 0x40) ? ~0 : 0;

            }
            break;
        }
        break;
    }
}

/* initialize time tables */
static void init_timetables(FM_OPN *OPN, double freqbase)
{
    int i,d;
    double rate;

    /* DeTune table */
    for (d = 0;d <= 3;d++)
    {
        for (i = 0;i <= 31;i++)
        {
            rate = ((double)dt_tab[d*32 + i]) * freqbase * (1<<(FREQ_SH-10)); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
            OPN->ST.dt_tab[d][i]   = (int32_t) rate;
            OPN->ST.dt_tab[d+4][i] = -OPN->ST.dt_tab[d][i];
        }
    }

    /* there are 2048 FNUMs that can be generated using FNUM/BLK registers
    but LFO works with one more bit of a precision so we really need 4096 elements */
    /* calculate fnumber -> increment counter table */
    for(i = 0; i < 4096; i++)
    {
        /* freq table for octave 7 */
        /* OPN phase increment counter = 20bit */
        /* the correct formula is : F-Number = (144 * fnote * 2^20 / M) / 2^(B-1) */
        /* where sample clock is  M/144 */
        /* this means the increment value for one clock sample is FNUM * 2^(B-1) = FNUM * 64 for octave 7 */
        /* we also need to handle the ratio between the chip frequency and the emulated frequency (can be 1.0)  */
        OPN->fn_table[i] = (uint32_t)( (double)i * 32 * freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
    }

    /* maximal frequency is required for Phase overflow calculation, register size is 17 bits (Nemesis) */
    OPN->fn_max = (uint32_t)( (double)0x20000 * freqbase * (1<<(FREQ_SH-10)) );
}

/// Set pre-scaler and make time tables.
///
/// @param OPN the OPN emulator to set the pre-scaler and create timetables for
///
static void OPNSetPres(FM_OPN *OPN) {
    // frequency base
    OPN->ST.freqbase = (OPN->ST.rate) ? ((double)OPN->ST.clock / OPN->ST.rate) : 0;
    // TODO: why is it necessary to scale these increments by a factor of 1/16
    //       to get the correct timings from the EG and LFO?
    // EG timer increment (updates every 3 samples)
    OPN->eg_timer_add = (1 << EG_SH) * OPN->ST.freqbase / 16;
    OPN->eg_timer_overflow = 3 * (1 << EG_SH) / 16;
    // LFO timer increment (updates every 16 samples)
    OPN->lfo_timer_add  = (1 << LFO_SH) * OPN->ST.freqbase / 16;
    // make time tables
    init_timetables(OPN, OPN->ST.freqbase);
}

/// initialize generic tables.
static void init_tables(void) {
    signed int i,x;
    signed int n;
    double o,m;

    /* build Linear Power Table */
    for (x=0; x<TL_RES_LEN; x++) {
        m = (1<<16) / pow(2, (x+1) * (ENV_STEP/4.0) / 8.0);
        m = floor(m);

        /* we never reach (1<<16) here due to the (x+1) */
        /* result fits within 16 bits at maximum */

        n = (int)m;     /* 16 bits here */
        n >>= 4;        /* 12 bits here */
        if (n&1)        /* round to nearest */
            n = (n>>1)+1;
        else
            n = n>>1;
                        /* 11 bits here (rounded) */
        n <<= 2;        /* 13 bits here (as in real chip) */

        /* 14 bits (with sign bit) */
        tl_tab[ x*2 + 0 ] = n;
        tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

        /* one entry in the 'Power' table use the following format, xxxxxyyyyyyyys with:            */
        /*        s = sign bit                                                                      */
        /* yyyyyyyy = 8-bits decimal part (0-TL_RES_LEN)                                            */
        /* xxxxx    = 5-bits integer 'shift' value (0-31) but, since Power table output is 13 bits, */
        /*            any value above 13 (included) would be discarded.                             */
        for (i=1; i<13; i++) {
            tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
            tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 + i*2*TL_RES_LEN ];
        }
    }

    /* build Logarithmic Sinus table */
    for (i=0; i<SIN_LEN; i++) {
        /* non-standard sinus */
        m = sin( ((i*2)+1) * M_PI / SIN_LEN ); /* checked against the real chip */
        /* we never reach zero here due to ((i*2)+1) */

        if (m>0.0)
            o = 8*log(1.0/m)/log(2.0);  /* convert to 'decibels' */
        else
            o = 8*log(-1.0/m)/log(2.0); /* convert to 'decibels' */

        o = o / (ENV_STEP/4);

        n = (int)(2.0*o);
        if (n&1)            /* round to nearest */
            n = (n>>1)+1;
        else
            n = n>>1;

        /* 13-bits (8.5) value is formatted for above 'Power' table */
        sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );
    }

    /* build LFO PM modulation table */
    for(i = 0; i < 8; i++) {  /* 8 PM depths */
        uint8_t fnum;
        for (fnum=0; fnum<128; fnum++) {  /* 7 bits meaningful of F-NUMBER */
            uint8_t value;
            uint8_t step;
            uint32_t offset_depth = i;
            uint32_t offset_fnum_bit;
            uint32_t bit_tmp;
            for (step=0; step<8; step++) {
                value = 0;
                for (bit_tmp=0; bit_tmp<7; bit_tmp++) {  /* 7 bits */
                    if (fnum & (1<<bit_tmp)) {  /* only if bit "bit_tmp" is set */
                        offset_fnum_bit = bit_tmp * 8;
                        value += lfo_pm_output[offset_fnum_bit + offset_depth][step];
                    }
                }
                /* 32 steps for LFO PM (sinus) */
                lfo_pm_table[(fnum*32*8) + (i*32) + step   + 0] = value;
                lfo_pm_table[(fnum*32*8) + (i*32) +(step^7)+ 8] = value;
                lfo_pm_table[(fnum*32*8) + (i*32) + step   +16] = -value;
                lfo_pm_table[(fnum*32*8) + (i*32) +(step^7)+24] = -value;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// MARK: YM2612 Emulator class
// ---------------------------------------------------------------------------

YamahaYM2612::YamahaYM2612(double clock_rate, double sample_rate) {
    // allocate total level table (128kb space)
    init_tables();
    OPN.P_CH = CH;
    OPN.type = TYPE_YM2612;
    OPN.ST.clock = clock_rate;
    OPN.ST.rate = sample_rate;
    reset();
}

void YamahaYM2612::setSampleRate(double clock_rate, double sample_rate) {
    OPN.ST.clock = clock_rate;
    OPN.ST.rate = sample_rate;
    OPNSetPres(&OPN);
}

void YamahaYM2612::reset() {
    // clear instance variables
    memset(registers, 0, sizeof registers);
    LFO = MOL = MOR = 0;
    // set the frequency scaling parameters of the OPN emulator
    OPNSetPres(&OPN);
    // status clear
    FM_IRQMASK_SET(&(OPN.ST), 0x03);
    // mode 0 , timer reset
    OPNWriteMode(&OPN, 0x27, 0x30);
    // envelope generator
    OPN.eg_timer = 0;
    OPN.eg_cnt = 0;
    // LFO
    OPN.lfo_timer = 0;
    OPN.lfo_cnt = 0;
    OPN.LFO_AM = 126;
    OPN.LFO_PM = 0;
    // state
    OPN.ST.status = 0;
    OPN.ST.mode = 0;

    OPNWriteMode(&OPN, 0x27, 0x30);
    OPNWriteMode(&OPN, 0x26, 0x00);
    OPNWriteMode(&OPN, 0x25, 0x00);
    OPNWriteMode(&OPN, 0x24, 0x00);

    reset_channels(&(OPN.ST), &CH[0], 6);

    for (int i = 0xb6; i >= 0xb4; i--) {
        OPNWriteReg(&OPN, i, 0xc0);
        OPNWriteReg(&OPN, i | 0x100, 0xc0);
    }

    for (int i = 0xb2; i >= 0x30; i--) {
        OPNWriteReg(&OPN, i, 0);
        OPNWriteReg(&OPN, i | 0x100, 0);
    }

    // DAC mode clear
    is_DAC_enabled = 0;
    out_DAC = 0;
    for (int c = 0; c < 6; c++) setST(c, 3);
}

void YamahaYM2612::step() {
    int lt, rt;
    // refresh PG and EG
    refresh_fc_eg_chan(&OPN, &CH[0]);
    refresh_fc_eg_chan(&OPN, &CH[1]);
    refresh_fc_eg_chan(&OPN, &CH[2]);
    refresh_fc_eg_chan(&OPN, &CH[3]);
    refresh_fc_eg_chan(&OPN, &CH[4]);
    refresh_fc_eg_chan(&OPN, &CH[5]);
    // clear outputs
    OPN.out_fm[0] = 0;
    OPN.out_fm[1] = 0;
    OPN.out_fm[2] = 0;
    OPN.out_fm[3] = 0;
    OPN.out_fm[4] = 0;
    OPN.out_fm[5] = 0;
    // update SSG-EG output
    update_ssg_eg_channel(&(CH[0].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[1].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[2].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[3].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[4].SLOT[SLOT1]));
    update_ssg_eg_channel(&(CH[5].SLOT[SLOT1]));
    // calculate FM
    chan_calc(&OPN, &CH[0]);
    chan_calc(&OPN, &CH[1]);
    chan_calc(&OPN, &CH[2]);
    chan_calc(&OPN, &CH[3]);
    chan_calc(&OPN, &CH[4]);
    if (is_DAC_enabled)
        *&CH[5].connect4 += out_DAC;
    else
        chan_calc(&OPN, &CH[5]);
    // advance LFO
    advance_lfo(&OPN);
    // advance envelope generator
    OPN.eg_timer += OPN.eg_timer_add;
    while (OPN.eg_timer >= OPN.eg_timer_overflow) {
        OPN.eg_timer -= OPN.eg_timer_overflow;
        OPN.eg_cnt++;
        advance_eg_channel(&OPN, &(CH[0].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[1].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[2].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[3].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[4].SLOT[SLOT1]));
        advance_eg_channel(&OPN, &(CH[5].SLOT[SLOT1]));
    }
    // clip outputs
    if (OPN.out_fm[0] > 8191)
        OPN.out_fm[0] = 8191;
    else if (OPN.out_fm[0] < -8192)
        OPN.out_fm[0] = -8192;
    if (OPN.out_fm[1] > 8191)
        OPN.out_fm[1] = 8191;
    else if (OPN.out_fm[1] < -8192)
        OPN.out_fm[1] = -8192;
    if (OPN.out_fm[2] > 8191)
        OPN.out_fm[2] = 8191;
    else if (OPN.out_fm[2] < -8192)
        OPN.out_fm[2] = -8192;
    if (OPN.out_fm[3] > 8191)
        OPN.out_fm[3] = 8191;
    else if (OPN.out_fm[3] < -8192)
        OPN.out_fm[3] = -8192;
    if (OPN.out_fm[4] > 8191)
        OPN.out_fm[4] = 8191;
    else if (OPN.out_fm[4] < -8192)
        OPN.out_fm[4] = -8192;
    if (OPN.out_fm[5] > 8191)
        OPN.out_fm[5] = 8191;
    else if (OPN.out_fm[5] < -8192)
        OPN.out_fm[5] = -8192;
    // 6-channels mixing
    lt = ((OPN.out_fm[0] >> 0) & OPN.pan[0]);
    rt = ((OPN.out_fm[0] >> 0) & OPN.pan[1]);
    lt += ((OPN.out_fm[1] >> 0) & OPN.pan[2]);
    rt += ((OPN.out_fm[1] >> 0) & OPN.pan[3]);
    lt += ((OPN.out_fm[2] >> 0) & OPN.pan[4]);
    rt += ((OPN.out_fm[2] >> 0) & OPN.pan[5]);
    lt += ((OPN.out_fm[3] >> 0) & OPN.pan[6]);
    rt += ((OPN.out_fm[3] >> 0) & OPN.pan[7]);
    lt += ((OPN.out_fm[4] >> 0) & OPN.pan[8]);
    rt += ((OPN.out_fm[4] >> 0) & OPN.pan[9]);
    lt += ((OPN.out_fm[5] >> 0) & OPN.pan[10]);
    rt += ((OPN.out_fm[5] >> 0) & OPN.pan[11]);
    // output buffering
    MOL = lt;
    MOR = rt;
    // timer A control
    if ((OPN.ST.TAC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
        TimerAOver(&OPN.ST);
    // timer B control
    if ((OPN.ST.TBC -= static_cast<int>(OPN.ST.freqbase * 4096)) <= 0)
        TimerBOver(&OPN.ST);
}

void YamahaYM2612::write(uint8_t a, uint8_t v) {
    int addr;
    // adjust to 8 bit bus
    v &= 0xff;
    switch (a & 3) {
    case 0:  // address port 0
        OPN.ST.address = v;
        addr_A1 = 0;
        break;

    case 1:  // data port 0
        // verified on real YM2608
        if (addr_A1 != 0) break;

        addr = OPN.ST.address;
        registers[addr] = v;
        switch (addr & 0xf0) {
        case 0x20:  // 0x20-0x2f Mode
            switch (addr) {
            case 0x2a:  // DAC data (YM2612), level unknown
                out_DAC = ((int)v - 0x80) << 6;
                break;
            case 0x2b:  // DAC Sel (YM2612), b7 = dac enable
                is_DAC_enabled = v & 0x80;
                break;
            default:  // OPN section, write register
                OPNWriteMode(&OPN, addr, v);
            }
            break;
        default:  // 0x30-0xff OPN section, write register
            OPNWriteReg(&OPN, addr, v);
        }
        break;

    case 2: // address port 1
        OPN.ST.address = v;
        addr_A1 = 1;
        break;

    case 3: // data port 1
        // verified on real YM2608
        if (addr_A1 != 1) break;

        addr = OPN.ST.address;
        registers[addr | 0x100] = v;
        OPNWriteReg(&OPN, addr | 0x100, v);
        break;
    }
}

void YamahaYM2612::setAR(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].AR == value) return;
    channels[channel].operators[slot].AR = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->ar_ksr = (s->ar_ksr&0xC0)|(value&0x1f);
    set_ar_ksr(&CH[channel], s, s->ar_ksr);
}

/* set decay rate */
void YamahaYM2612::setD1(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].D1 == value) return;
    channels[channel].operators[slot].D1 = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->dr = (s->dr&0x80)|(value&0x1F);
    set_dr(s, s->dr);
}

void YamahaYM2612::setSL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].SL == value) return;
    channels[channel].operators[slot].SL = value;
    FM_SLOT *s =  &CH[channel].SLOT[slots_idx[slot]];
    s->sl_rr = (s->sl_rr&0x0f)|((value&0x0f)<<4);
    set_sl_rr(s, s->sl_rr);
}

/* set sustain rate */
void YamahaYM2612::setD2(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].D2 == value) return;
    channels[channel].operators[slot].D2 = value;
    set_sr(&CH[channel].SLOT[slots_idx[slot]], value);
}

void YamahaYM2612::setRR(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].RR == value) return;
    channels[channel].operators[slot].RR = value;
    FM_SLOT *s =  &CH[channel].SLOT[slots_idx[slot]];
    s->sl_rr = (s->sl_rr&0xf0)|(value&0x0f);
    set_sl_rr(s, s->sl_rr);
}

void YamahaYM2612::setTL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].TL == value) return;
    channels[channel].operators[slot].TL = value;
    set_tl(&CH[channel], &CH[channel].SLOT[slots_idx[slot]], value);
}

void YamahaYM2612::setMUL(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].MUL == value) return;
    channels[channel].operators[slot].MUL = value;
    CH[channel].SLOT[slots_idx[slot]].mul = (value&0x0f)? (value&0x0f)*2 : 1;
    CH[channel].SLOT[SLOT1].Incr=-1;
}

void YamahaYM2612::setDET(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].DET == value) return;
    channels[channel].operators[slot].DET = value;
    CH[channel].SLOT[slots_idx[slot]].DT  = OPN.ST.dt_tab[(value)&7];
    CH[channel].SLOT[SLOT1].Incr=-1;
}

void YamahaYM2612::setRS(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].RS == value) return;
    channels[channel].operators[slot].RS = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->ar_ksr = (s->ar_ksr&0x1F)|((value&0x03)<<6);
    set_ar_ksr(&CH[channel], s, s->ar_ksr);
}

void YamahaYM2612::setAM(uint8_t channel, uint8_t slot, uint8_t value) {
    if (channels[channel].operators[slot].AM == value) return;
    channels[channel].operators[slot].AM = value;
    FM_SLOT *s = &CH[channel].SLOT[slots_idx[slot]];
    s->AMmask = (value) ? ~0 : 0;
}
