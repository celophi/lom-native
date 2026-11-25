#ifndef LOM_NATIVE_WMAP_MATH_H
#define LOM_NATIVE_WMAP_MATH_H

#include <libgte.h>

/**
 * @brief Psy-Q's CompMatrix, which PSY-Z's function of that name doesn't match:
 *        combine two matrices with the GTE's own operations.
 * @param left The first matrix.
 * @param right The second.
 * @param output Set to the combination.
 * @return @p output.
 */
MATRIX* lom_native_wmap_CompMatrix(MATRIX* left, MATRIX* right, MATRIX* output);

#endif
