
#include "pmeter.h"
#include <math.h>
#include <stdlib.h>

#if (_PMETER_RTOS == 0)
#define pmeter_delay(x)  HAL_Delay(x)
#elif (_PMETER_RTOS == 1)
#include "cmsis_os.h"
#include "freertos.h"
#define pmeter_delay(x)  osDelay(x)
#else
#include "cmsis_os2.h"
#include "freertos.h"
#define _PMETER_delay(x)  osDelay(x)
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
  _PMETER_TIM.Instance->ARR = (_PMETER_SAMPLE / 2) - 1;
  HAL_TIM_Base_Start(&_PMETER_TIM);
  HAL_ADC_Start_DMA(&_PMETER_ADC, (uint32_t*)pmeter.buff[pmeter.buff_inedex], _PMETER_SAMPLE * 2);
  pmeter.v_ratio = (_PMETER_REFERENCE / (float)(1 << _PMETER_ADC_BIT)) * (1.0f / (_PMETER_VDIV_R2 / (_PMETER_VDIV_R1 + _PMETER_VDIV_R2)));
  pmeter.i_ratio = (_PMETER_REFERENCE / (float) (1 << _PMETER_ADC_BIT)) * ((float) _PMETER_CT_RATIO / (float) _PMETER_CT_RESISTOR);
  pmeter.calib.w = 1.0f;
  pmeter.calib.v = 1.0f;
  pmeter.calib.i = 1.0f;
  pmeter.calib.offset = (uint16_t)(1 << _PMETER_ADC_BIT) / 2;
  pmeter_printf("[pmeter] init done. timer frequency: %d , period : %d ms\r\n", timer_freq_mhz, _PMETER_SAMPLE);
}
//###################################################################################################
void pmeter_loop(void)
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
      v_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_V - 1] - pmeter.calib.offset;
      v_sum_sq += (v_signed * v_signed);
      i_signed = pmeter.buff[1 - pmeter.buff_inedex][i + _PMETER_RANK_I - 1] - pmeter.calib.offset;
      if (i_signed > _PMETER_ADC_NOISE_VALUE || i_signed < -_PMETER_ADC_NOISE_VALUE)
      {
        i_sum_sq += (i_signed * i_signed);
      }
      p_sum += i_signed * v_signed;
    }
    rms = (float)sqrt((float)v_sum_sq / (float)_PMETER_SAMPLE) * pmeter.v_ratio;
    pmeter.v = rms;
    rms = (float) sqrt((float) i_sum_sq / (float) _PMETER_SAMPLE) * pmeter.i_ratio;
    pmeter.i = rms;
    pmeter.va = pmeter.v * pmeter.i;
    pmeter.w = ((float)p_sum / (float)_PMETER_SAMPLE) * pmeter.v_ratio * pmeter.i_ratio * pmeter.calib.w;
    if (pmeter.w > pmeter.va)
      pmeter.w = pmeter.va;
    pmeter.pf = pmeter.w / pmeter.va;
    if (pmeter.pf < 0.001f)
      pmeter.pf = 0.001f;
    pmeter.fi = 360.0f * (acos(pmeter.pf) / (2.0f*3.14159265f));
    pmeter.var = pmeter.v * pmeter.i * sin(pmeter.fi);
    pmeter.vah += pmeter.va / 3600.0f;
    pmeter.wh += pmeter.w / 3600.0f;
    pmeter.varh += pmeter.var / 3600.0f;
  }
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
  pmeter.buff_done = 0;
  while (pmeter.buff_done == 0)
    pmeter_delay(1);
  uint32_t sum = 0;
  for (uint16_t i = _PMETER_RANK_I - 1; i < _PMETER_SAMPLE * 2; i += 2)
    sum += pmeter.buff[1 - pmeter.buff_inedex][i];
  sum = sum / _PMETER_SAMPLE;
  pmeter_printf("[pmeter] pmeter.calib.offset: %d\r\n", (uint16_t )sum);
  pmeter_calib->offset = sum;
  pmeter.calib.offset = sum;
  pmeter_loop();
  float error = rms_voltage / pmeter.v;
  pmeter_printf("[pmeter] pmeter.calib.v: %.5f\r\n", error);
  pmeter_calib->v = error;
}
//###################################################################################################
void pmeter_calib_step2_res_load(pmeter_calib_t *pmeter_calib, float rms_current)
{
  pmeter_printf("[pmeter] calibration step 2, resistor load, rms current: %0.2f\r\n", rms_current);
  uint32_t start = HAL_GetTick();
  while (HAL_GetTick() - start < 3000)
  {
    pmeter_loop();
    pmeter_delay(1);
  }
  float error = rms_current / pmeter.i;
  pmeter_printf("[pmeter] pmeter.calib.i: %.5f\r\n", error);
  pmeter_calib->i = error;
  error = pmeter.va / pmeter.w;
  pmeter_printf("[pmeter] pmeter.calib.w: %.5f\r\n", error);
  pmeter_calib->w = error;
}
//###################################################################################################
void pmeter_calib_set(pmeter_calib_t pmeter_calib)
{
  pmeter.calib.offset = pmeter_calib.offset;
  pmeter.calib.v = pmeter_calib.v;
  pmeter.calib.i = pmeter_calib.i;
  pmeter.calib.w = pmeter_calib.w;
  pmeter.v_ratio *= pmeter_calib.v;
  pmeter.i_ratio *= pmeter_calib.i;
}
//###################################################################################################
