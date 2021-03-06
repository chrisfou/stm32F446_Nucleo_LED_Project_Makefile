#ifndef __BOOT_H__
#define __BOOT_H__

/* Includes-------------------------------------------------------------------*/

/* Defines -------------------------------------------------------------------*/

/* typedef -------------------------------------------------------------------*/

typedef struct
{
  uint8_t* pt_arg;
  uint16_t len;
} ARG_t;

typedef uint8_t* (*PT_FUNC_CMD)(uint8_t, ARG_t*);

/* Constantes ----------------------------------------------------------------*/

/* Public function specificaition  -------------------------------------------*/

#endif	/* #define __BOOT_H__ */
