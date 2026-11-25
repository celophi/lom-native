#include "sdk/kernel.h"

int PS1_EnterCriticalSection(void);
void PS1_ExitCriticalSection(void);
#define EnterCriticalSection PS1_EnterCriticalSection
#define ExitCriticalSection PS1_ExitCriticalSection
