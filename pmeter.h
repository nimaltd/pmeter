
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
 * Version: 1.2.1
 *
 * History:
 * (1.2.1): Add pmeter_pause(), pmeter_resume(), pmeter_set_counter() functions.
 * (1.2.0): Change to 2 point calibration. Fix some bugs.
 * (1.1.0): Bug fixed. Do not need ratio of resistors and CT.
 * (1.0.4): Improve VAR Accuracy.
 * (1.0.3): Fix NaN values.
 * (1.0.2): Fix calibration.
 * (1.0.1): Fix configuration timer value.
 * (1.0.0): First release.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint16_t    center;
  float       v_raw_low;
  float       v_raw_high;
  float       i_raw_low;
  float       i_raw_high;
  float       w_raw_low;
  float       w_raw_high;
  float       v_ref_low;
  float       v_ref_high;
  float       i_ref_low;
  float       i_ref_high;
  float       w_ref_low;
  float       w_ref_high;
  
}pmeter_calib_t;

typedef struct
{
  uint16_t        buff[2][_PMETER_SAMPLE * 2];
  uint8_t         buff_inedex;
  uint8_t         buff_done;
  pmeter_calib_t  calib;
  float           v_raw;
  float           i_raw;
  float           w_raw;
  float           v;
  float           i;
  float           va;
  float           w;
  float           var;
  float           vah;
  float           wh;
  float           varh;
  float           pf;
  float           fi;

}pmeter_t;

extern pmeter_t         pmeter;

//############################################################################################
void    pmeter_callback(void); // adc dma callback
void    pmeter_init(uint16_t timer_freq_mhz, pmeter_calib_t pmeter_calib); //  init power meter
void    pmeter_resume(void);  // resume 
void    pmeter_pause(void);   //  pause
uint8_t pmeter_loop(void);  //  after update value, return 1
void    pmeter_reset_counter(void); //  reset all wh,vah,varh
void    pmeter_set_counter(float vah, float wh, float varh); // set init value

void    pmeter_calib_step1_without_load_v_high(float rms_voltage);
void    pmeter_calib_step2_without_load_v_low(float rms_voltage);
void    pmeter_calib_step3_resistor_load_i_high(float rms_current);
void    pmeter_calib_step4_resistor_load_i_low(float rms_current);

void    pmeter_calib_load(pmeter_calib_t pmeter_calib); // load calibration values
//############################################################################################
#ifdef __cplusplus
}
#endif
#endif /* _PMETER_H_ */
