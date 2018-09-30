/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdlib.h>
#include "main.h"
#include "boot.h"


/* Private variables ---------------------------------------------------------*/


/* Public function body ------------------------------------------------------*/

uint8_t* LED_CMD(uint8_t nb_args, ARG_t* args)
{
  uint8_t* l_result = NULL;
  uint8_t l_led = 0;
  uint8_t l_etat = 0;
  uint8_t* l_pt_char = NULL;

  if (nb_args != 3)
    {
      l_result = (uint8_t*)"nb_args error";
    }

  l_led = strtoul((char*)args[1].pt_Arg, 
		  (char**)&l_pt_char, 
		  10);
  l_etat = strtoul((char*)args[2].pt_Arg, 
		   (char**)&l_pt_char, 
		   10);
  
  switch (l_led)
    {
    case 1:
      switch (l_etat) 
        {         
        case 0:
          HAL_GPIO_WritePin(LED_EXTERNAL_2_GPIO_Port, 
			    LED_EXTERNAL_2_Pin, 
			    GPIO_PIN_RESET);
          break;
        case 1:
          HAL_GPIO_WritePin(LED_EXTERNAL_2_GPIO_Port, 
			    LED_EXTERNAL_2_Pin, 
			    GPIO_PIN_SET);
          break;
        default:
          break;
        }
      break; 
    case 2:
      switch (l_etat) 
        {         
        case 0:
          HAL_GPIO_WritePin(LED_EXTERNAL_GPIO_Port, 
			    LED_EXTERNAL_Pin, 
			    GPIO_PIN_RESET);
          break;
        case 1:
          HAL_GPIO_WritePin(LED_EXTERNAL_GPIO_Port, 
			    LED_EXTERNAL_Pin, 
			    GPIO_PIN_SET);
          break;
        default:
          break;
        }
      break;
    default:
      break;
    }

  return l_result;

}
