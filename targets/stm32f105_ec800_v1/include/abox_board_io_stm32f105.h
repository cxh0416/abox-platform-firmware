#ifndef ABOX_BOARD_IO_STM32F105_H
#define ABOX_BOARD_IO_STM32F105_H

#include "abox_board_io.h"

/* Called by generated GPIO initialization before any board output is enabled. */
void ABoxBoardIoStm32_SafeInit(void);
const ABoxBoardIoPort *ABoxBoardIoStm32_Port(void);

#endif
