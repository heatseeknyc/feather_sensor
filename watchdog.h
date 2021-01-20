#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "transmit.h"

void watchdog_init();
void watchdog_feed();
void watchdog_disable();
void watchdog_kill();

#endif
