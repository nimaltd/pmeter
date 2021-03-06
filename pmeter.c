
#include "pmeter.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if (_PMETER_RTOS == 0)
#define pmeter_delay(x)  HAL_Delay(x)
#else
#include "cmsis_os.h"
#include "freertos.h"
#define pmeter_delay(x)  osDelay(x)
#endif
#if (_PMETER_DEBUG == 1)
#include <stdio.h>
#define pmeter_printf(...)     printf(__VA_ARGS__)
#else
#define pmeter_printf(...)     {};
#endif

extern TIM_HandleTypeDef _PMETER_TIM;
extern ADC_HandleTypeDef _PMETER_ADC;

pmeter_t        pmeter;
 
//###################################################################################################
void pmeter_callback(void)
{
  if (pmeter.buff_inedex == 0)
  {
    pmeter.buff_done = 1;
    pmeter.buff_inedex = 1;
  }
  else
  {
    pmeter.buff_done = 1;
    pmeter.buff_inedex = 0;
  }
  HAL_ADC_Start_DMA(&_PMETER_ADC, (uint32_t*)pmeter.buff[pmeter.buff_inedex], _PMETER_SAMPLE * 2);
}
//###################################################################################################
void pmeter_init(uint16_t timer_freq_mhz, pmeter_calib_t pmeter_calib)
{
  _PMETER_TIM.Instance->PSC = timer_freq_mhz - 1;
  _PMETER_TIM.Instance->ARR = (uint32_t)((float)_PMETER_SAMPLE / (((float)_PMETER_SAMPLE / 1000.0f) * ((float)_PMETER_SAMPLE / 1000.0f))) - 1;
  memcpy(&pmeter.calib, &pmeter_calib, sizeof(pmeter_calib_t));
  HAL_TIM_Base_Start(&_PMETER_TIM);
  HAL_ADC_Start_DMA(&_PMETER_ADC, (uint32_t*)pmeter.buff[pmeter.buff_inedex], _PMETER_SAMPLE * 2);
  pmeter_printf("[pmeter] init done. timer frequency: %d MHz\r\n", timer_freq_mhz);
}
//###################################################################################################
void pmeter_deinit(void)
{
  HAL_TIM_Base_Stop(&_PMETER_TIM);
  HAL_ADC_Stop_DMA(&_PMETER_ADC);
  pmeter_printf("[pmeter] deinit done.\r\n");
}
//###################################################################################################
uint8_t pmeter_loop(void)
{
  if (pmeter.buff_done == 1)
  {
    pmeter.buff_done = 0;
    uint32_t v_sum_sq = 0, i_sum_sq = 0;
    int32_t p_sum = 0;
    for (uint16_t i = 0; i <= (_PMETER_SAMPLE * 2) - 2; i += 2)
    {
      int32_t v_signed, i_signed;
      v_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_V - 1] - pmeter.calib.center;
      v_sum_sq += (v_signed * v_signed);
      i_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_I - 1] - pmeter.calib.center;
      if (i_signed > _PMETER_ADC_NOISE_VALUE || i_signed < -_PMETER_ADC_NOISE_VALUE)
      {
        i_sum_sq += (i_signed * i_signed);
        p_sum += i_signed * v_signed;
      }
    }
    pmeter.v_raw = (float)sqrt((float)v_sum_sq / (float)_PMETER_SAMPLE);
    pmeter.v = (pmeter.v_raw - pmeter.calib.v_raw_low) * (pmeter.calib.v_ref_high - pmeter.calib.v_ref_low);
    pmeter.v /= (pmeter.calib.v_raw_high - pmeter.calib.v_raw_low);
    pmeter.v += pmeter.calib.v_ref_low;
    if (isinf(pmeter.v))
      pmeter.v = 0;
    if (pmeter.v < 0)
      pmeter.v = 0;
    pmeter.i_raw = (float) sqrt((float) i_sum_sq / (float) _PMETER_SAMPLE);
    pmeter.i = (pmeter.i_raw - pmeter.calib.i_raw_low) * (pmeter.calib.i_ref_high - pmeter.calib.i_ref_low);
    pmeter.i /= (pmeter.calib.i_raw_high - pmeter.calib.i_raw_low);
    pmeter.i += pmeter.calib.i_ref_low;
    if (isinf(pmeter.i))
      pmeter.i = 0;
    if (pmeter.i < 0)
      pmeter.i = 0;
    pmeter.va = pmeter.v * pmeter.i;
    pmeter.w_raw = (float)((float)p_sum / (float)_PMETER_SAMPLE);
    pmeter.w = (pmeter.w_raw - pmeter.calib.w_raw_low) * (pmeter.calib.w_ref_high - pmeter.calib.w_ref_low);
    pmeter.w /= (pmeter.calib.w_raw_high - pmeter.calib.w_raw_low);
    pmeter.w += pmeter.calib.w_ref_low;
    if (pmeter.w < 0)
      pmeter.w = 0;    
    if (pmeter.w > pmeter.va)
      pmeter.w = pmeter.va;
    pmeter.pf = pmeter.w / pmeter.va;
    if (isnan(pmeter.pf))
      pmeter.pf = 1.0f;
    pmeter.fi = 360.0f * (acosf(pmeter.pf) / (2.0f * 3.14159265f));
    pmeter.var = pmeter.v * pmeter.i * sinf(pmeter.fi);
    if (isnan(pmeter.var))
      pmeter.var = 0;
    pmeter.wh += pmeter.w / 3600.0f;
    pmeter.vah += pmeter.va / 3600.0f;
    pmeter.varh += pmeter.var / 3600.0f;
    return 1;
  }
  else
    return 0;
}
//###################################################################################################
void pmeter_reset_counter(void)
{
  pmeter.vah = 0;
  pmeter.wh = 0;
  pmeter.varh = 0;
}
//###################################################################################################
void pmeter_calib_step1_without_load_v_high(float rms_voltage)
{
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);  
  uint32_t sum = 0;
  for (uint16_t i = _PMETER_RANK_I - 1; i < _PMETER_SAMPLE * 2; i += 2)
    sum += pmeter.buff[1 - pmeter.buff_inedex][i];
  sum = sum / _PMETER_SAMPLE;
  pmeter.calib.center = sum;
  while (pmeter_loop() == 0)
    pmeter_delay(1); 
  pmeter.calib.v_ref_high = rms_voltage;
  pmeter.calib.v_raw_high = pmeter.v_raw;
}
//###################################################################################################
void pmeter_calib_step2_without_load_v_low(float rms_voltage)
{
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);  
  pmeter.calib.v_ref_low = rms_voltage;
  pmeter.calib.v_raw_low = pmeter.v_raw;
}
//###################################################################################################
void pmeter_calib_step3_resistor_load_i_high(float rms_current)
{
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);  
  pmeter.calib.i_ref_high = rms_current;
  pmeter.calib.i_raw_high = pmeter.i_raw;
  pmeter.calib.w_raw_high = pmeter.w_raw;
  pmeter.calib.w_ref_high = rms_current * pmeter.v;
}
//###################################################################################################
void pmeter_calib_step4_resistor_load_i_low(float rms_current)
{
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);  
  pmeter.calib.i_ref_low = rms_current;
  pmeter.calib.i_raw_low = pmeter.i_raw;
  pmeter.calib.w_raw_low = pmeter.w_raw;
  pmeter.calib.w_ref_low = rms_current * pmeter.v;
}
//###################################################################################################
void pmeter_calib_set(pmeter_calib_t pmeter_calib)
{
  memcpy(&pmeter.calib, &pmeter_calib, sizeof(pmeter_calib_t));
}
//###################################################################################################
