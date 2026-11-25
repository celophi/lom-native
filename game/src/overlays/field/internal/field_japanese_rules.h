#ifndef FIELD_JAPANESE_RULES_H
#define FIELD_JAPANESE_RULES_H

#include "lom_native_japanese_balance.h"

#if defined(VERSION_JP)
#define FIELD_JAPANESE_RULES() 1
#else
#define FIELD_JAPANESE_RULES() lom_japanese_balance()
#endif

#endif
