
#include "pmeter.h"
#include <math.h>
#include <stdlib.h>

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

pmeter_t  pmeter;

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
void pmeter_init(uint16_t timer_freq_mhz)
{
  _PMETER_TIM.Instance->PSC = timer_freq_mhz - 1;
  _PMETER_TIM.Instance->ARR = (uint32_t)((float)_PMETER_SAMPLE / (((float)_PMETER_SAMPLE / 1000.0f) * ((float)_PMETER_SAMPLE / 1000.0f))) - 1;
  HAL_TIM_Base_Start(&_PMETER_TIM);
  HAL_ADC_Start_DMA(&_PMETER_ADC, (uint32_t*)pmeter.buff[pmeter.buff_inedex], _PMETER_SAMPLE * 2);
  pmeter.v_ratio = 1.0f;
  pmeter.i_ratio = 1.0f;
  pmeter.w_ratio = 1.0f;
  pmeter.offset = 2048;
  pmeter_printf("[pmeter] init done. timer frequency: %d MHz\r\n", timer_freq_mhz);
}
//###################################################################################################
uint8_t pmeter_loop(void)
{
  if (pmeter.buff_done == 1)
  {
    pmeter.buff_done = 0;
    uint32_t v_sum_sq = 0, i_sum_sq = 0;
    int32_t p_sum = 0;
    float rms = 0;
    for (uint16_t i = 0; i <= (_PMETER_SAMPLE * 2) - 2; i += 2)
    {
      int32_t v_signed, i_signed;
      v_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_V - 1] - pmeter.offset;
      v_sum_sq += (v_signed * v_signed);
      i_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_I - 1] - pmeter.offset;
      if (i_signed > _PMETER_ADC_NOISE_VALUE || i_signed < -_PMETER_ADC_NOISE_VALUE)
      {
        i_sum_sq += (i_signed * i_signed);
        p_sum += i_signed * v_signed;
      }
    }
    rms = (float)sqrt((float)v_sum_sq / (float)_PMETER_SAMPLE) * pmeter.v_ratio;
    pmeter.v = rms;
    rms = (float) sqrt((float) i_sum_sq / (float) _PMETER_SAMPLE) * pmeter.i_ratio;
    pmeter.i = rms;
    pmeter.va = pmeter.v * pmeter.i;
    pmeter.w = ((float)p_sum / (float)_PMETER_SAMPLE) * pmeter.v_ratio * pmeter.i_ratio * pmeter.w_ratio;
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
void pmeter_calib_step1_no_load(pmeter_calib_t *pmeter_calib, float rms_voltage)
{
  pmeter_printf("[pmeter] calibration step 1, no load, rms voltage: %0.2f\r\n", rms_voltage);
  pmeter.v_ratio = 1.0f;
  pmeter.i_ratio = 1.0f;
  pmeter.w_ratio = 1.0f;
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);  
  uint32_t sum = 0;
  for (uint16_t i = _PMETER_RANK_I - 1; i < _PMETER_SAMPLE * 2; i += 2)
    sum += pmeter.buff[1 - pmeter.buff_inedex][i];
  sum = sum / _PMETER_SAMPLE;
  pmeter_printf("[pmeter] pmeter.offset: %d\r\n", (uint16_t )sum);
  pmeter_calib->offset = sum;
  pmeter.offset = sum;
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  float error = rms_voltage / pmeter.v;
  pmeter_calib->v = error * pmeter.v_ratio;
  pmeter.v_ratio = pmeter_calib->v;
  pmeter_printf("[pmeter] pmeter.v_ratio: %.5f\r\n", pmeter.v_ratio);
}
//###################################################################################################
void pmeter_calib_step2_res_load(pmeter_calib_t *pmeter_calib, float rms_current)
{
  pmeter_printf("[pmeter] calibration step 2, resistor load, rms current: %0.2f\r\n", rms_current);
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  float error = rms_current / pmeter.i;
  pmeter_calib->i = error * pmeter.i_ratio;
  pmeter.i_ratio = pmeter_calib->i;
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  while (pmeter_loop() == 0)
    pmeter_delay(1);
  error = pmeter.va / pmeter.w;
  pmeter_calib->w = error;
  pmeter.w_ratio = error;
  pmeter_printf("[pmeter] pmeter.i_ratio: %.5f\r\n", pmeter.i_ratio);
  pmeter_printf("[pmeter] pmeter.w_ratio: %.5f\r\n", pmeter.w_ratio);
}
//###################################################################################################
void pmeter_calib_set(pmeter_calib_t pmeter_calib)
{
  pmeter.v_ratio = pmeter_calib.v;
  pmeter.i_ratio = pmeter_calib.i;
  pmeter.w_ratio = pmeter_calib.w;
  pmeter.offset = pmeter_calib.offset;
}
//###################################################################################################
