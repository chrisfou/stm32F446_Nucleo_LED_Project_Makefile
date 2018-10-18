/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "boot.h"

/* External references -------------------------------------------------------*/
extern UART_HandleTypeDef huart2;

extern uint8_t *LED_CMD(uint8_t nb_args, ARG_t *args);

/* Defines -------------------------------------------------------------------*/
#define FIFO_LEN_MAX      256
#define CARRIAGE_RETURN   0x0D
#define NB_ARGS_MAX       5
#define NB_CMD_MAX        1

/* Typedef -------------------------------------------------------------------*/
typedef struct {
    uint16_t idx_unit16_in;
    uint16_t idx_uint16_out;
    uint8_t buffer[FIFO_LEN_MAX];
} FIFO_s;

typedef struct {
    uint8_t *pt_string_name;
    PT_FUNC_CMD pt_func_cmd;
} CMD_s;

typedef enum {
    WF_CHAR,
    WF_SPACE
} FSM_GET_ARG_s;

/* Constantes ----------------------------------------------------------------*/
uint8_t *const c_pt_string_prompt = (uint8_t *) "\n\rSTM32Boot> ";
uint8_t *const c_pt_string_msg_vt100_clear_screen = (uint8_t *) "\033[2J";
uint8_t *const c_pt_string_msg_cmd_rx_overflow = (uint8_t *) "Msg Cmd RX Overflow Error !";
uint8_t *const c_pt_string_msg_cmd_not_found = (uint8_t *) "Command not found!";

const CMD_s C_CMDS[NB_CMD_MAX] = {{(uint8_t *) "LED", LED_CMD}};

/* Private variables ---------------------------------------------------------*/
static FIFO_s g_fifo_tx;
static FIFO_s g_fifo_rx;
static uint8_t g_char_rx;
static uint8_t g_string_msg_cmd_rx[FIFO_LEN_MAX];
static uint16_t g_uint16_msg_cmd_rx_id;
static ARG_t g_args[NB_ARGS_MAX];
static uint8_t *g_pt_char;
static FSM_GET_ARG_s g_fsm_get_arg = WF_CHAR;
static uint8_t g_uint8_nb_arg;

/* Private function specification --------------------------------------------*/
static void FIFO_IncIdx(uint16_t *pt_idx);

static void FIFO_Init(FIFO_s *pt_fifo);

static void FIFO_PushChar(FIFO_s *pt_fifo, uint8_t thechar);

static uint8_t *FIFO_PopChar(FIFO_s *pt_fifo);

static void UART_SendStringOnIt(uint8_t *msg, uint16_t len);

/* Public function body ------------------------------------------------------*/
void BOOT_Init(void) {
    FIFO_Init(&g_fifo_tx);
    FIFO_Init(&g_fifo_rx);
    g_uint16_msg_cmd_rx_id = 0;

    UART_SendStringOnIt(c_pt_string_msg_vt100_clear_screen,
                        (uint16_t) strlen((char *) c_pt_string_msg_vt100_clear_screen));

    UART_SendStringOnIt(c_pt_string_prompt,
                        (uint16_t) strlen((char *) c_pt_string_prompt));

    HAL_UART_Receive_IT(&huart2,
                        &g_char_rx, 1);
}

void BOOT_LoopStart(void) {

    uint8_t *l_cmd_result = NULL;
    uint8_t l_cmd_exe_flag = 0;

    /* Lancement de la boucle sans fin */
    while (1) {

        /* Test présence d'un caractère dans la fifo de reception de l'uart */
        while ((g_pt_char = FIFO_PopChar(&g_fifo_rx)) != NULL) {

            /* Un caractère a été détecté
             * La présence d'un caractère 0x0D indique de la part
             * de l'utilisation au souhait d'executer une commande */
            switch (*g_pt_char) {

                case CARRIAGE_RETURN:

                    /* Initialisation du tableau g_args avant parsing de
                     * la ligne de commande */
                    for (uint8_t id_arg = 0; id_arg < NB_ARGS_MAX; id_arg++) {
                        g_args[id_arg].len = 0;
                        g_args[id_arg].pt_arg = NULL;
                    }

                    /* Parsing de la ligne de commande.
                     * Les espaces sont ignorés. Les références et les longueurs
                     * des arguments sont enregistrées dans le tableau g_args  */
                    g_uint8_nb_arg = 0;
                    g_fsm_get_arg = WF_CHAR;
                    for (uint8_t id_char = 0; id_char < g_uint16_msg_cmd_rx_id; id_char++) {
                        switch (g_fsm_get_arg) {
                            case WF_CHAR:

                                /* Les espaces sont ignorés pour la recherche
                                 * des arguments */
                                if (isspace(g_string_msg_cmd_rx[id_char]) == 0) {
                                    g_uint8_nb_arg++;
                                    g_args[g_uint8_nb_arg - 1].pt_arg = g_string_msg_cmd_rx + id_char;
                                    g_args[g_uint8_nb_arg - 1].len++;
                                    g_fsm_get_arg = WF_SPACE;
                                }
                                break;

                            case WF_SPACE:

                                /* Les espaces sont ignorés pour la recherche
                                 * des arguments */
                                if (isspace(g_string_msg_cmd_rx[id_char]) == 0) {
                                    g_args[g_uint8_nb_arg - 1].len++;
                                } else {
                                    g_fsm_get_arg = WF_CHAR;
                                }
                                break;
                            default:
                                break;
                        }
                    }

                    /* Les chaines de caracteres correspondant aux arguments sont
                     * terminées par des '0' pour permettrent aux commandes de parser
                     * les arguments à l'aide des fonctions de traitement
                     * de chaine de caractères classiques */
                    for (uint8_t id_arg = 0; id_arg < NB_ARGS_MAX; id_arg++) {

                        /* Seules les arguments recus sont terminées par un zero */
                        if (g_args[id_arg].pt_arg != NULL) {
                            *(g_args[id_arg].pt_arg + g_args[id_arg].len) = 0;
                        }
                    }

                    /* Le traitement de la commande ne s'effectue uniquement
                     * si le nombre d'argument est non nul. */
                    if (g_uint8_nb_arg > 0) {

                        /* Recherche de la fonction associée à la commande.
                         * Par principe le premier argument correspond au nom
                         * de la fonction a executer. */
                        l_cmd_exe_flag = 0;

                        for (uint8_t id_cmd = 0; id_cmd < NB_CMD_MAX; id_cmd++) {
                            if (strncmp((char *) C_CMDS[id_cmd].pt_string_name,
                                        (char *) g_args[0].pt_arg,
                                        g_args[0].len) == 0) {
                                /* Un flag est levé pour memoriser le traitement de la commande. */
                                l_cmd_exe_flag = 1;

                                /* Execution de la commande */
                                l_cmd_result =
                                        (*C_CMDS[id_cmd].pt_func_cmd)(g_uint8_nb_arg, g_args);

                                /* Test le retour de la commande et affiche le message si besoin. */
                                if (l_cmd_result != NULL) {
                                    UART_SendStringOnIt(c_pt_string_prompt,
                                                        (uint16_t) strlen((char *) c_pt_string_prompt));
                                    UART_SendStringOnIt(l_cmd_result,
                                                        (uint16_t) strlen((char *) l_cmd_result));
                                    UART_SendStringOnIt(c_pt_string_prompt,
                                                        (uint16_t) strlen((char *) c_pt_string_prompt));
                                }
                                break;
                            }
                        }

                        /* Si aucune commande de correspond alors affichage
                           d'un message d'erreur */
                        if (l_cmd_exe_flag == 0) {
                            UART_SendStringOnIt(c_pt_string_prompt,
                                                (uint16_t) strlen((char *) c_pt_string_prompt));
                            UART_SendStringOnIt(c_pt_string_msg_cmd_not_found,
                                                (uint16_t) strlen((char *) c_pt_string_msg_cmd_not_found));
                        }
                    }

                    /* Affichage par défaut du prompt sur receptiond'un retour chariot */
                    UART_SendStringOnIt(c_pt_string_prompt,
                                        (uint16_t) strlen((char *) c_pt_string_prompt));

                    /* Le buffer de reception est vidé pour permettre la reception d'une nouvelle commande
                     * par l'utilisateur.*/
                    g_uint16_msg_cmd_rx_id = 0;
                    break;

                default:

                    /* Le caractère recu est mémorisé dans le buffer de commande */
                    g_string_msg_cmd_rx[g_uint16_msg_cmd_rx_id++] = *g_pt_char;

                    /* Si le buffer de commande est plein alors emission d'un message d'erreur */
                    if (g_uint16_msg_cmd_rx_id == FIFO_LEN_MAX) {
                        g_uint16_msg_cmd_rx_id = 0;
                        UART_SendStringOnIt(c_pt_string_msg_vt100_clear_screen,
                                            (uint16_t) strlen((char *) c_pt_string_msg_vt100_clear_screen));
                        UART_SendStringOnIt(c_pt_string_prompt,
                                            (uint16_t) strlen((char *) c_pt_string_prompt));
                        UART_SendStringOnIt(c_pt_string_msg_cmd_rx_overflow,
                                            (uint16_t) strlen((char *) c_pt_string_msg_cmd_rx_overflow));
                        UART_SendStringOnIt(c_pt_string_prompt,
                                            (uint16_t) strlen((char *) c_pt_string_prompt));
                    }

                    /* Echo a la console du caractère recu */
                    UART_SendStringOnIt(g_pt_char, 1);

                    break;
            }
        }
    }
}

/* Private function declaration ----------------------------------------------*/

static void FIFO_IncIdx(uint16_t *pt_idx) {
    if (pt_idx != NULL) {
        (*pt_idx)++;

        if (*pt_idx == FIFO_LEN_MAX) {
            *pt_idx = 0;
        }
    }
}

static void FIFO_Init(FIFO_s *pt_fifo) {
    pt_fifo->idx_unit16_in = 0;
    pt_fifo->idx_uint16_out = 0;
}

static void FIFO_PushChar(FIFO_s *pt_fifo, uint8_t thechar) {
    pt_fifo->buffer[pt_fifo->idx_unit16_in] = thechar;

    FIFO_IncIdx(&(pt_fifo->idx_unit16_in));

    if (pt_fifo->idx_uint16_out == pt_fifo->idx_unit16_in) {
        FIFO_IncIdx(&(pt_fifo->idx_uint16_out));
    }
}

static uint8_t *FIFO_PopChar(FIFO_s *pt_fifo) {
    uint8_t *l_result = NULL;

    if (pt_fifo->idx_unit16_in != pt_fifo->idx_uint16_out) {
        l_result = pt_fifo->buffer + pt_fifo->idx_uint16_out;

        FIFO_IncIdx(&(pt_fifo->idx_uint16_out));

    }

    return l_result;
}

static void UART_SendStringOnIt(uint8_t *msg, uint16_t len) {
    if ((len > 0) && msg != NULL) {
        for (uint16_t i = 0; i < len; i++) {
            FIFO_PushChar(&g_fifo_tx, msg[i]);
        }

        while ((HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX) ||
               (HAL_UART_GetState(&huart2) == HAL_UART_STATE_BUSY_TX_RX)) {
            HAL_Delay(100);
        }

        HAL_UART_Transmit_IT(&huart2,
                             FIFO_PopChar(&g_fifo_tx), 1);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    FIFO_PushChar(&g_fifo_rx, g_char_rx);

    /* be ready to receive a new character */
    HAL_UART_Receive_IT(&huart2, &g_char_rx, 1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    uint8_t *l_pt_char_tx;

    l_pt_char_tx = FIFO_PopChar(&g_fifo_tx);

    if (l_pt_char_tx != NULL) {
        HAL_UART_Transmit_IT(&huart2,
                             l_pt_char_tx, 1);
    }
}
