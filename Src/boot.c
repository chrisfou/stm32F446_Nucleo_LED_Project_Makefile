/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "boot.h"

/* External references ------------------------------------------------------*/

extern UART_HandleTypeDef huart2;

/* Defines ------------------------------------------------------------------*/

#define FIFO_LEN_MAX      256
#define CARRIAGE_RETURN   0x0D
#define NB_ARGS_MAX       3
#define NB_CMD_MAX        1

/* typedef ------------------------------------------------------------------*/

typedef struct
{
  uint16_t idx_in;
  uint16_t idx_out;
  uint8_t buffer[FIFO_LEN_MAX];

} FIFO_t;

typedef struct
{
  uint8_t* Pt_CmdName;
  Pt_FuncCmd_t Pt_FuncCmd;
  uint8_t NbFuncCmdPar;
} CMD_t;

typedef enum
  {
    WF_CHAR,
    WF_SPACE
  } FSM_GET_ARG_t;

/* Constantes ----------------------------------------------------------------*/

uint8_t* const C_Msg_Prompt = (uint8_t*)"\n\rSTM32Boot> ";
uint8_t* const C_Msg_VT100_Clear_Screen = (uint8_t*)"\033[2J";
uint8_t* const C_Msg_Cmd_Rx_Overflow = (uint8_t*)"Msg Cmd RX Overflow Error !";
uint8_t* const C_Msg_Nb_Args_Error = (uint8_t*)"Nb Args. Error !";
uint8_t* const C_Msg_Cmd_Not_Found = (uint8_t*)"Command not found!";

extern uint8_t* LED_CMD(uint8_t nb_args, ARG_t* args);
const CMD_t C_CMDS[NB_CMD_MAX] = {{(uint8_t*)"LED", LED_CMD, 1}};

/* Private variables ---------------------------------------------------------*/

static FIFO_t G_Fifo_TX;
static FIFO_t G_Fifo_RX;
static uint8_t G_Rx_Char;
static uint8_t G_Rx_Msg_Cmd[FIFO_LEN_MAX];
static uint16_t G_Rx_Msg_Cmd_Id;
static ARG_t G_ARGS[NB_ARGS_MAX];
static uint8_t* G_pt_char;
static FSM_GET_ARG_t G_FSM_GET_ARG = WF_CHAR;
static uint8_t G_nb_arg;

/* Private function specification -----------------------------------------------*/

static void FIFO_Init(FIFO_t *pt_fifo);
static void FIFO_PushChar(FIFO_t *pt_fifo, uint8_t thechar);
static uint8_t* FIFO_PopChar(FIFO_t *pt_fifo);
static void UART_SendStringOnIt (uint8_t* msg, uint16_t len);

/* Public function body --------------------------------------------------------*/

void BOOT_Init(void)
{
  FIFO_Init(&G_Fifo_TX);
  FIFO_Init(&G_Fifo_RX);
  G_Rx_Msg_Cmd_Id = 0;

  UART_SendStringOnIt(C_Msg_VT100_Clear_Screen,
		      strlen((char*)C_Msg_VT100_Clear_Screen));

  UART_SendStringOnIt(C_Msg_Prompt, 
		      strlen((char*)C_Msg_Prompt));
  
  HAL_UART_Receive_IT(&huart2, 
		      &G_Rx_Char, 1);
}

void BOOT_LoopStart(void)
{

  uint8_t* l_cmd_result = NULL;
  uint8_t  l_cmd_exe_flag = 0;

  /* Lancement de la boucle sans fin */
  while(1)
    {

      /* Test présence d'un caractère dans la fifo de reception de l'uart */
      while ((G_pt_char = FIFO_PopChar(&G_Fifo_RX)) != NULL)
	{

	  /* Un caractère a été détecté.
	     La présence d'un caractère 0x0D indique de la part de l'utilisation au souhait
	     d'executer une commande */
	  switch (*G_pt_char)
	    {

	    case CARRIAGE_RETURN:
	      
	      /* Initialisation du tableau G_ARGS avant parsing de la ligne de commande */
	      for (uint8_t id_arg =0; id_arg < NB_ARGS_MAX; id_arg++)
		{
		  G_ARGS[id_arg].len=0;
		  G_ARGS[id_arg].pt_Arg = NULL;
		}
	      
	      /* Parsing de la ligne de commande.
		 Les espaces sont ignorés.
		 Les références et les longueurs des arguments sont enregistrées dans le tableau G_ARGS  */
	      G_nb_arg = 0;
	      G_FSM_GET_ARG = WF_CHAR;
	      for (uint8_t id_char =0; id_char < G_Rx_Msg_Cmd_Id; id_char++)
		{
		  switch (G_FSM_GET_ARG)
		    {
		    case WF_CHAR:
		      
		      /* Les espaces sont ignorés pour la recherche des arguments */
		      if (isspace(G_Rx_Msg_Cmd[id_char]) ==0 )
			{
			  G_nb_arg++;
			  G_ARGS[G_nb_arg-1].pt_Arg = G_Rx_Msg_Cmd + id_char;
			  G_ARGS[G_nb_arg-1].len++;
			  G_FSM_GET_ARG = WF_SPACE;
			}
		      break;
		    
		    case WF_SPACE:
		      
		      /* Les espaces sont ignorés pour la recherche des arguments */
		      if (isspace(G_Rx_Msg_Cmd[id_char]) ==0)
			{
			  G_ARGS[G_nb_arg-1].len++;
			}
		      else
			{
			  G_FSM_GET_ARG = WF_CHAR;
			}
		      break;
		    default:
		      break;
		    }
		}

	      /* Les chaines de caracteres correspondant aux arguments sont
		 terminées par des '0' pour permettrent aux commandes de parser
		 les arguments à l'aide des fonctions de traitement de chaine de 
		 caractères classiques */
	      for (uint8_t id_arg =0; id_arg < NB_ARGS_MAX; id_arg++)
		{
		  
		  /* Seules les arguments recus sont terminées par un zero */
		  if (G_ARGS[id_arg].pt_Arg == NULL)
		    {
		      *(G_ARGS[id_arg].pt_Arg + G_ARGS[id_arg].len) = 0;
		    }
		}
	  
	      /* Le traitement de la commande ne s'effectue uniquement si le nombre
		 d'argument est non nul. */
	      if (G_nb_arg > 0)
		{

		  /* Recherche de la fonction associée à la commande.
		     Par principe le premier argument correspond au nom de la fonction a executer. */
		  l_cmd_exe_flag = 0;

		  for (uint8_t id_cmd=0; id_cmd <  NB_CMD_MAX; id_cmd ++)
		    {   
		      if (strncmp((char*)C_CMDS[id_cmd].Pt_CmdName, 
				  (char*)G_ARGS[0].pt_Arg, 
				  G_ARGS[0].len) == 0)
			{
			  /* Un flag est levé pour memoriser le traitement de la commande. */
			  l_cmd_exe_flag = 1;

			  /* Execution de la commande */
			  l_cmd_result = (*C_CMDS[id_cmd].Pt_FuncCmd)(G_nb_arg, G_ARGS);

			  /* Test le retour de la commande et affiche le message si besoin. */
			  if (l_cmd_result != NULL)
			    {
			      UART_SendStringOnIt(C_Msg_Prompt, 
						  strlen((char*)C_Msg_Prompt));
			      UART_SendStringOnIt(l_cmd_result, 
						  strlen((char*)l_cmd_result));
			      UART_SendStringOnIt(C_Msg_Prompt, 
						  strlen((char*)C_Msg_Prompt));
			    }
   			  break;
			}		      
		    }

		  /* Si aucune commande de correspond alors affichage d'un message d'erreur */
		  if (l_cmd_exe_flag == 0)
		    {
		      UART_SendStringOnIt(C_Msg_Prompt, 
					  strlen((char*)C_Msg_Prompt));
		      UART_SendStringOnIt(C_Msg_Cmd_Not_Found, 
					  strlen((char*)C_Msg_Cmd_Not_Found));
		    }
		}
	      
	      /* Affichage par défaut du prompt sur reception d'un retour chariot */
	      UART_SendStringOnIt(C_Msg_Prompt, 
				  strlen((char*)C_Msg_Prompt));

	      /* Le buffer de reception est vidé pour permettre la reception d'une nouvelle commande
		 par l'utilisateur.*/
	      G_Rx_Msg_Cmd_Id=0;
	      break;
	  
	    default:

	      /* Le caractère recu est mémorisé dans le buffer de commande */
	      G_Rx_Msg_Cmd[G_Rx_Msg_Cmd_Id++] = *G_pt_char;

	      /* Si le buffer de commande est plein alors emission d'un message d'erreur */
	      if (G_Rx_Msg_Cmd_Id == FIFO_LEN_MAX)
		{
		  G_Rx_Msg_Cmd_Id = 0;
		  UART_SendStringOnIt(C_Msg_VT100_Clear_Screen, 
				      strlen((char*)C_Msg_VT100_Clear_Screen));
		  UART_SendStringOnIt(C_Msg_Prompt, 
				      strlen((char*)C_Msg_Prompt));
		  UART_SendStringOnIt(C_Msg_Cmd_Rx_Overflow, 
				      strlen((char*)C_Msg_Cmd_Rx_Overflow));
		  UART_SendStringOnIt(C_Msg_Prompt, 
				      strlen((char*)C_Msg_Prompt));
		}

	      /* Echo a la console du caractère recu */
	      UART_SendStringOnIt(G_pt_char, 1);

	      break;
	    }
	}
    }
}

/* Private function body --------------------------------------------------------*/
 
static void FIFO_Init(FIFO_t *pt_fifo)
{
  pt_fifo->idx_in  = 0;
  pt_fifo->idx_out = 0;
}

static void FIFO_PushChar (FIFO_t *pt_fifo, uint8_t thechar)
{
  pt_fifo->buffer[pt_fifo->idx_in++] = thechar;

  if (pt_fifo->idx_in == FIFO_LEN_MAX)
    {
      pt_fifo->idx_in = 0;
    }

  if (pt_fifo->idx_out == pt_fifo->idx_in)
    {
      pt_fifo->idx_out++;
      if (pt_fifo->idx_out == FIFO_LEN_MAX)
        {
          pt_fifo->idx_out=0;
        }
    }
}

static uint8_t* FIFO_PopChar (FIFO_t *pt_fifo)
{
  uint8_t* l_result = NULL;

  if (pt_fifo->idx_in != pt_fifo->idx_out)
    {
      l_result = pt_fifo->buffer + pt_fifo->idx_out++;
    }

  if (pt_fifo->idx_out == FIFO_LEN_MAX)
    {
      pt_fifo->idx_out=0;
    }

  return l_result;
}

static void UART_SendStringOnIt (uint8_t* msg, uint16_t len)
{
  if ((len > 0) && msg != NULL)
    {
      for (uint16_t i = 0; i < len ; i++)
        {
          FIFO_PushChar(&G_Fifo_TX, msg[i]);
        }

      while ((HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX ) ||
             (HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX_RX ))
        {
          HAL_Delay(100);
        }
      HAL_UART_Transmit_IT(&huart2, 
			   (uint8_t *)FIFO_PopChar(&G_Fifo_TX), 1);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  FIFO_PushChar(&G_Fifo_RX, G_Rx_Char);

  /* be ready to receive a new character */
  HAL_UART_Receive_IT(&huart2, &G_Rx_Char, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t* l_pt_char_tx;

  l_pt_char_tx = FIFO_PopChar(&G_Fifo_TX);

  if ( l_pt_char_tx != NULL )
    {
      HAL_UART_Transmit_IT(&huart2, 
			   (uint8_t*)l_pt_char_tx, 1);
    }
}
