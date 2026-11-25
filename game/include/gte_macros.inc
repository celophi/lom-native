.ifndef .L_GTE_MACRO_INC
.L_GTE_MACRO_INC:

## GTE instruction macros
## These are meant for use with GAS and replace DMPSX

.macro cop2op fake_op, op, gbg = 0, sf = 1, mx = 0, v = 0, cv = 0, lm = 0
    cop2 \fake_op << 20 | \gbg << 20 | \sf << 19 | \mx << 17 | \v << 15 | \cv << 13 | \lm << 10 | \op
.endm

.macro rtps
    cop2op 0x01, 0x01
.endm

.macro rtpt
    cop2op 0x02, 0x30
.endm

.macro dpcl
    cop2op 0x06, 0x29
.endm

.macro dpcs
    cop2op 0x07, 0x10
.endm

.macro dpct
    cop2op 0x0F, 0x2A
.endm

.macro intpl
    cop2op 0x09, 0x11
.endm

.macro ncs
    cop2op 0x0C, 0x1E, lm = 1
.endm

.macro nct
    cop2op 0x0D, 0x20, lm = 1
.endm

.macro ncds
    cop2op 0x0E, 0x13, lm = 1
.endm

.macro ncdt
    cop2op 0x0F, 0x16, lm = 1
.endm

.macro nccs
    cop2op 0x10, 0x1B, lm = 1
.endm

.macro ncct
    cop2op 0x11, 0x3F, lm = 1
.endm

.macro cdp
    cop2op 0x12, 0x14, lm = 1
.endm

.macro cc
    cop2op 0x13, 0x1C, lm = 1
.endm

.macro nclip
    cop2op 0x14, 0x06, sf = 0
.endm

.macro avsz3
    cop2op 0x15, 0x2D
.endm

.macro avsz4
    cop2op 0x16, 0x2E
.endm


## Instructions which take an argument
#gbg : arg is 5 bit wide
#sf : arg is 1 bit wide
#mx : arg is 2 bit wide
#v : arg is 2 bit wide
#cv : arg is 2 bit wide
#lm : arg is 1 bit wide

.macro mvmva sf, mx, v, cv, lm
    cop2op 0x04, 0x12, sf = \sf, mx = \mx, v = \v, cv = \cv, lm = \lm
.endm

.macro sqr sf
    cop2op 0x0A, 0x28, sf = \sf, lm = 1
.endm

.macro op sf
    cop2op 0x17, 0x0C, sf = \sf
.endm

.macro gpf sf
    cop2op 0x19, 0x3D, sf = \sf
.endm

.macro gpl sf
    cop2op 0x1A, 0x3E, sf = \sf
.endm


## Convenience macros

.macro rtv0
#.word 0x4A486012
    mvmva       1, 0, 0, 3, 0
.endm

.macro rtv1
#.word 0x4A48E012
    mvmva       1, 0, 1, 3, 0
.endm

.macro rtv2
#.word 0x4A496012
    mvmva       1, 0, 2, 3, 0
.endm

.macro rtir12
#.word 0x4A49E012
    mvmva       1, 0, 3, 3, 0
.endm

.macro rtir0
#.word 0x4A41E012
    mvmva       0, 0, 3, 3, 0
.endm

.macro rtv0tr
#.word 0x4A480012
    mvmva       1, 0, 0, 0, 0
.endm

.macro rtv1tr
#.word 0x4A488012
    mvmva       1, 0, 1, 0, 0
.endm

.macro rtv2tr
#.word 0x4A490012
    mvmva       1, 0, 2, 0, 0
.endm

.macro rtirtr
#.word 0x4A498012
    mvmva       1, 0, 3, 0, 0
.endm

.macro rtv0bk
#.word 0x4A482012
    mvmva       1, 0, 0, 1, 0
.endm

.macro rtv1bk
#.word 0x4A48A012
    mvmva       1, 0, 1, 1, 0
.endm

.macro rtv2bk
#.word 0x4A492012
    mvmva       1, 0, 2, 1, 0
.endm

.macro rtirbk
#.word 0x4A49A012
    mvmva       1, 0, 3, 1, 0
.endm

.macro ll
#.word 0x4A4A6412
    mvmva       1, 1, 0, 3, 1
.endm

.macro llv0
#.word 0x4A4A6012
    mvmva       1, 1, 0, 3, 0
.endm

.macro llv1
#.word 0x4A4AE012
    mvmva       1, 1, 1, 3, 0
.endm

.macro llv2
#.word 0x4A4B6012
    mvmva       1, 1, 2, 3, 0
.endm

.macro llvir
#.word 0x4A4BE012
    mvmva       1, 1, 3, 3, 0
.endm

.macro llv0tr
#.word 0x4A4A0012
    mvmva       1, 1, 0, 0, 0
.endm

.macro llv1tr
#.word 0x4A4A8012
    mvmva       1, 1, 1, 0, 0
.endm

.macro llv2tr
#.word 0x4A4B0012
    mvmva       1, 1, 2, 0, 0
.endm

.macro llirtr
#.word 0x4A4B8012
    mvmva       1, 1, 3, 0, 0
.endm

.macro llv0bk
#.word 0x4A4A2012
    mvmva       1, 1, 0, 1, 0
.endm

.macro llv1bk
#.word 0x4A4AA012
    mvmva       1, 1, 1, 1, 0
.endm

.macro llv2bk
#.word 0x4A4B2012
    mvmva       1, 1, 2, 1, 0
.endm

.macro llirbk
#.word 0x4A4BA012
    mvmva       1, 1, 3, 1, 0
.endm

.macro lc
#.word 0x4A4DA412
    mvmva       1, 2, 3, 1, 1
.endm

.macro lcv0
#.word 0x4A4C6012
    mvmva       1, 2, 0, 3, 0
.endm

.macro lcv1
#.word 0x4A4CE012
    mvmva       1, 2, 1, 3, 0
.endm

.macro lcv2
#.word 0x4A4D6012
    mvmva       1, 2, 2, 3, 0
.endm

.macro lcvir
#.word 0x4A4DE012
    mvmva       1, 2, 3, 3, 0
.endm

.macro lcv0tr
#.word 0x4A4C0012
    mvmva       1, 2, 0, 0, 0
.endm

.macro lcv1tr
#.word 0x4A4C8012
    mvmva       1, 2, 1, 0, 0
.endm

.macro lcv2tr
#.word 0x4A4D0012
    mvmva       1, 2, 2, 0, 0
.endm

.macro lcirtr
#.word 0x4A4D8012
    mvmva       1, 2, 3, 0, 0
.endm

.macro lev0bk
#.word 0x4A4C2012
    mvmva       1, 2, 0, 1, 0
.endm

.macro lev1bk
#.word 0x4A4CA012
    mvmva       1, 2, 1, 1, 0
.endm

.macro lev2bk
#.word 0x4A4D2012
    mvmva       1, 2, 2, 1, 0
.endm

.macro leirbk
#.word 0x4A4DA012
    mvmva       1, 2, 3, 1, 0
.endm

#.macro sqr12
# #.word 0x4AA80428
#sqr 1
#.endm

#.macro sqr0
# #.word 0x4AA00428
#sqr 0
#.endm

.macro op12
#.word 0x4B78000C
    op          1
.endm

.macro op0
#.word 0x4B70000C
    op          0
.endm

.macro gpf12
#.word 0x4B98003D
    gpf         1
.endm

.macro gpf0
#.word 0x4B90003D
    gpf         0
.endm

.macro gpl12
#.word 0x4BA8003E
    gpl         1
.endm

.macro gpl0
#.word 0x4BA0003E
    gpl         0
.endm

.endif
