#ifndef SDK_SPADSTK_H
#define SDK_SPADSTK_H

#define SetSpadStack(save_slot)                                                                                                                                \
    __asm__ volatile("addu $8,%0,$0\n\t"                                                                                                                       \
                     "sw $29,0($8)\n\t"                                                                                                                        \
                     "addiu $8,$8,-4\n\t"                                                                                                                      \
                     "addu $29,$8,$0"                                                                                                                          \
                     :                                                                                                                                         \
                     : "r"(save_slot)                                                                                                                          \
                     : "$8", "memory")

#define ResetSpadStack()                                                                                                                                       \
    __asm__ volatile("addiu $29,$29,4\n\t"                                                                                                                     \
                     "lw $29,0($29)"                                                                                                                           \
                     :                                                                                                                                         \
                     :                                                                                                                                         \
                     : "$29", "memory")

#endif
