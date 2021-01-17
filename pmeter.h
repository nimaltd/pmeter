
#ifndef _PMETER_H_
#define _PMETER_H_
#include "main.h"
#include "pmeterConfig.h"

/*
 *  AC power meter library based on STM32 internal ADC
 *
 *  Author:     Nima Askari
 *  WebSite:    https://www.github.com/NimaLTD
 *  Instagram:  https://www.instagram.com/github.NimaLTD
 *  LinkedIn:   https://www.linkedin.com/in/NimaLTD
 *  Youtube:    https://www.youtube.com/channel/UCUhY7qY1klJm1d2kulr9ckw
 */

/*
 * Version: 1.0.1
 *
 * History:
 *
 * (1.0.1): fix configuration timer value.
 * (1.0.0): First release.
 */

typedef struct
{
  uint16_t    offset;
  float       v;
  float       i;
  float       w;

}pmeter_calib_t;

typedef struct
{
  uint16_t        buff[2][_PMETER_SAMPLE * 2];
  uint8_t         buff_inedex;
  uint8_t         buff_done;
  pmeter_calib_t  calib;

  float           v_ratio;
  float           i_ratio;
  float           v;
  float           i;
  float           va;
  float           vah;
  float           w;
  float           wh;
  float           var;
  float           varh;
  float           pf;
  float           fi;

}pmeter_t;

extern pmeter_t pmeter;
//############################################################################################
void          pmeter_callback(void);
void          pmeter_init(uint16_t timer_freq_mhz);
void          pmeter_loop(void);
void          pmeter_reset_counter(void);

void          pmeter_calib_step1_no_load(pmeter_calib_t *pmeter_calib, float rms_voltage);  //  no load
void          pmeter_calib_step2_res_load(pmeter_calib_t *pmeter_calib, float rms_current); //  resistor load with nominal current
void          pmeter_calib_set(pmeter_calib_t pmeter_calib);
//############################################################################################
#endif /* _PMETER_H_ */
