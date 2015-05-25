#ifndef __TIME_H__
#define __TIME_H__

extern inline void sleep(unsigned int sec);
extern inline void msleep(unsigned int ms);

extern inline unsigned int set_timeout(unsigned int ms);
extern inline int is_timeout(unsigned int goal);

#include <kernel/jiffies.h>

#endif /* __TIME_H__ */