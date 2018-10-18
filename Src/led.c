/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdlib.h>
#include "main.h"
#include "boot.h"

/* Externals references ------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;

/* Private variables ---------------------------------------------------------*/
uint8_t G_Led_1_blinking = 0;
uint8_t G_Led_2_blinking = 0;

/* Public function body ------------------------------------------------------*/
uint8_t* LED_CMD(uint8_t nb_args, ARG_t* pt_args)
{
  uint8_t* l_pt_result = NULL;
  uint8_t l_led = 0;
  uint8_t l_etat = 0;
  uint32_t l_period = 0;
   
  /* The LED_CMD can accept up to 4 args :
     arg n� 0 -> cmd name,
     arg n� 1 -> led identification,
     arg n� 3 -> status,
     arg n� 4 -> blinking period */
  if (nb_args > 4)
    {
      l_pt_result = (uint8_t*)"nb args error";
      return l_pt_result;
    }
   
  /* First arg about the LED identification */
  l_led = (uint8_t) strtoul((char*)pt_args[1].pt_arg,
                            NULL,
                            10);
   
  /* An error is raised if the LED identification is not correct */
  if  ( (l_led < 1 ) || (l_led > 2) )
    {
      l_pt_result = (uint8_t*)"LED's Id not inside the range !";
      return l_pt_result;
    }
   
  /* Second arg about the LED state */
  l_etat = (uint8_t) strtoul((char*)pt_args[2].pt_arg,
                             NULL,
                             10);
   
  /* An error is raised if the LED status is not correct :
     0 -> off
     1 -> on
     2 -> blinking */
  if  ( (l_etat < 0) || (l_etat > 3) )
    {
      l_pt_result = (uint8_t*)"LED's Status not inside the range !";
      return l_pt_result;
    }

  if ( (nb_args < 4) && (l_etat == 2) )
    {
      l_pt_result = (uint8_t*)"LED's blinking period parameter is missing !";
      return l_pt_result;
    }

    if ((nb_args == 4) && (l_etat ==2))
    {
      /* third arg about the LED blinking period */
      l_period = strtoul((char*)pt_args[3].pt_arg,
                         NULL,
                         10);
       
      /* An error is raised if the LED status is not correct */
      if  ( (l_period < 10) || (l_period > 5000) )
        {
          l_pt_result = (uint8_t*)"LED's period not inside the range !";
          return l_pt_result;
        }
    }

  /* The received command with corrects args is applied */
  switch ((l_led << 4) + l_etat)
    {
    case 0x10:
      G_Led_1_blinking =0;
      HAL_GPIO_WritePin(LED_EXTERNAL_2_GPIO_Port, 
                        LED_EXTERNAL_2_Pin, 
                        GPIO_PIN_RESET);
      break;
       
    case 0x11:
      G_Led_1_blinking =0;
      HAL_GPIO_WritePin(LED_EXTERNAL_2_GPIO_Port, 
                        LED_EXTERNAL_2_Pin, 
                        GPIO_PIN_SET);
      break;
      
    case 0x20:
      G_Led_2_blinking =0;
      HAL_GPIO_WritePin(LED_EXTERNAL_GPIO_Port, 
                        LED_EXTERNAL_Pin, 
                        GPIO_PIN_RESET);
      break;
      
    case 0x21:
      G_Led_2_blinking =0;
      HAL_GPIO_WritePin(LED_EXTERNAL_GPIO_Port, 
                        LED_EXTERNAL_Pin, 
                        GPIO_PIN_SET);
      break;
      
    case 0x12 :
      G_Led_1_blinking =1;
      htim6.Init.Period = l_period;
      HAL_TIM_Base_Stop_IT(&htim6);
      HAL_TIM_Base_Init(&htim6);
      HAL_TIM_Base_Start_IT(&htim6);
      break;
      
    case 0x22 :
      G_Led_2_blinking =1;
      htim6.Init.Period = l_period;
      HAL_TIM_Base_Stop_IT(&htim6);
      HAL_TIM_Base_Init(&htim6);
      HAL_TIM_Base_Start_IT(&htim6);
      break;
       
    default:
      l_pt_result = (uint8_t*)"args error";
      break;
    }

  return l_pt_result;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
 if (G_Led_2_blinking)
 {
  HAL_GPIO_TogglePin(LED_EXTERNAL_GPIO_Port, LED_EXTERNAL_Pin);
 }
 if (G_Led_1_blinking)
 {
  HAL_GPIO_TogglePin(LED_EXTERNAL_2_GPIO_Port, LED_EXTERNAL_2_Pin);
 }
}
