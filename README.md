# power meter

*	Author:     Nima Askari
*	WebSite:    https://www.github.com/NimaLTD
*	Instagram:  https://www.instagram.com/github.NimaLTD
*	LinkedIn:   https://www.linkedin.com/in/NimaLTD
*	Youtube:    https://www.youtube.com/channel/UCUhY7qY1klJm1d2kulr9ckw 

AC power meter based on STM32 ADC
* Setlect a ADC and enable 2 channel, v and i.
* Set number of conversion to 2.
* Set enable scan conversion.
* Set disable continous conversion.
* Set disable discontinous conversion.
* Set disable DMA continous conversion.
* Set end of single conversion.
* Select trigger out event a timer for external trigger.
* Select trigger rising edge.
* Select both ranks with correct channel.
* Set enable DMA normal mode.
* Set internal clock for selected timer.
* Set update event on trigger evnet selection on timer configuration menu.
* Add library on project.
* Configure "pmeterConfig.h".
* Put "pmeter_callback()" in adc dma callback.
* Call "pmeter_init(timer freq: ex:32)".
* Put "pmeter_loop()" in infinite loop.
* before using, you should be calibrate.
* Call "pmeter_calib_step1_no_load" and "pmeter_calib_step2_res_load" and store the calibration data.
```
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  pmeter_callback();
}

void main()
{
  pmeter_calib_t calib;
  pmeter_init(32);
  //pmeter_calib_step1_no_load(&calib, 24.5f); // 24.5v measure by ac multimeter
  //pmeter_calib_step2_res_load(&calib, 0.650f);// put a resistor and measure by ac multimeter
  calib.offset = 1980;
  calib.v = 0.9794f;
  calib.i = 0.9393f;
  calib.w = 1.030f;
  pmeter_calib_set(calib);
  while(1)
  {
    pmeter_loop();
  }
}
```




