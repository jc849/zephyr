/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FUN_ID_H__
#define __FUN_ID_H__

/* Function ID */
enum npcm4xx_fun_id_e {
	FUN_NONE = -1,
#define FUN_DEFINE(fun, ...) fun,
	#include "fun_def_list.h"
#undef FUN_DEFINE
	MAX_FUN_ID,
};

#endif /* #ifndef __FUN_ID_H__ */
