#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "task.h"

void scheduler_init(void);
void scheduler_add(struct task_struct *task);
void scheduler_start(void);
void scheduler_tick(void);      // Called from timer IRQ
void schedule(void);            // Pick next task and switch

#endif /* SCHEDULER_H */
