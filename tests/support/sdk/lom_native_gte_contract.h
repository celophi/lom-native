#include <stddef.h>
#define GTE_CALL(name, type) _Static_assert(_Generic(&(name), type: 1, default: 0), #name " native call ABI")
GTE_CALL(InitGeom, void (*)(void));

GTE_CALL(RotMatrix, MATRIX* (*)(SVECTOR*, MATRIX*));

GTE_CALL(RotMatrixX, MATRIX* (*)(long, MATRIX*));

GTE_CALL(RotMatrixY, MATRIX* (*)(long, MATRIX*));

GTE_CALL(RotMatrixZ, MATRIX* (*)(long, MATRIX*));

GTE_CALL(TransMatrix, MATRIX* (*)(MATRIX*, VECTOR*));

GTE_CALL(ScaleMatrix, MATRIX* (*)(MATRIX*, VECTOR*));

GTE_CALL(SetRotMatrix, void (*)(MATRIX*));

GTE_CALL(SetTransMatrix, void (*)(MATRIX*));

GTE_CALL(PushMatrix, void (*)(void));

GTE_CALL(PopMatrix, void (*)(void));

GTE_CALL(SetGeomOffset, void (*)(long, long));

GTE_CALL(SetGeomScreen, void (*)(long));

GTE_CALL(NormalClip, long (*)(long, long, long));

GTE_CALL(VectorNormalS, long (*)(VECTOR*, SVECTOR*));

GTE_CALL(SquareRoot0, long (*)(long));

GTE_CALL(rcos, int (*)(int));

GTE_CALL(rsin, int (*)(int));

GTE_CALL(ratan2, long (*)(long, long));
#undef GTE_CALL
_Static_assert(sizeof(MATRIX) == 32 && offsetof(MATRIX, t) == 20, "matrix layout");

_Static_assert(sizeof(VECTOR) == 16 && offsetof(VECTOR, vz) == 8 && offsetof(VECTOR, pad) == 12, "vector layout");

_Static_assert(sizeof(SVECTOR) == 8 && offsetof(SVECTOR, vz) == 4, "short vector layout");

_Static_assert(sizeof(DVECTOR) == 4 && sizeof(CVECTOR) == 4, "small vector layouts");
