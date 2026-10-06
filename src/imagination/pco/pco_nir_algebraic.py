# Copyright © 2025 Imagination Technologies Ltd.
# SPDX-License-Identifier: MIT

import argparse
import sys
import math

a = 'a'
b = 'b'

lower_algebraic = []
lower_algebraic_late = []

def lowered_fround_even(src):
   abs_src = ('fabs', src)
   ffloor_temp = ('ffloor', abs_src)
   ffract_temp = ('ffract', abs_src)

   ceil_temp = ('fadd', ffloor_temp, 1.0)
   even_temp = ('fmul', ffloor_temp, 0.5)

   even_ffract_temp = ('ffract', even_temp)

   ishalf_temp = ('feq', ffract_temp, 0.5)
   ffract_temp = ('bcsel', ishalf_temp, even_ffract_temp, ffract_temp)

   lesshalf_temp = ('flt', ffract_temp, 0.5)
   result_temp = ('bcsel', lesshalf_temp, ffloor_temp, ceil_temp)

   res = ('fcopysign_pco', result_temp, src)
   return res

lower_algebraic.append((('fround_even', a), lowered_fround_even(a)))

# Since fceil/ffloor preserve signed-zero, this ftrunc lowering does so too.
lower_ftrunc = [
    (('ftrunc', a) , ('bcsel', ('flt', a, 0.0), ('fceil', a), ('ffloor', a)))
]      

lower_algebraic.extend(lower_ftrunc)

lower_insert_extract = [
   (('insert_u8', 'a@32', b), ('bitfield_insert', 0, a, ('imul', b, 8), 8)),
   (('insert_u16', 'a@32', b), ('bitfield_insert', 0, a, ('imul', b, 16), 16)),

   (('extract_u8', 'a@32', b), ('ubitfield_extract', a, ('imul', b, 8), 8)),
   (('extract_i8', 'a@32', b), ('ibitfield_extract', a, ('imul', b, 8), 8)),

   (('extract_u16', 'a@32', b), ('ubitfield_extract', a, ('imul', b, 16), 16)),
   (('extract_i16', 'a@32', b), ('ibitfield_extract', a, ('imul', b, 16), 16)),
]

lower_algebraic_late.extend(lower_insert_extract)

lower_b2b = [
   (('b2b32', a), ('ineg', ('b2i32', a))),
   (('b2b1', a), ('ine', a, 0)),
]

lower_algebraic.extend(lower_b2b)

# Integer clamps of a floored float converted back to float, as in
# float(clamp(int(floor(log2(x))) + c, lo, hi)), stay exact in float:
# - i2f32 is monotonic, so it commutes with integer min/max;
# - floor(log2(x)) is within [-149, 128] (f2i32 of inf/NaN is undefined),
#   so adding a constant of magnitude at most 2^24 neither wraps nor leaves
#   the range where float and integer additions round the same exact sum;
# - i2f32(f2i32(floor(x))) is floor(x) wherever f2i32 is defined.
fold_float_int_roundtrip = [
   (('i2f32', ('imin', 'a@32', '#b')), ('fmin', ('i2f32', a), ('i2f32', b))),
   (('i2f32', ('imax', 'a@32', '#b')), ('fmax', ('i2f32', a), ('i2f32', b))),
   (('i2f32', ('iadd', ('f2i32', ('ffloor', ('flog2', 'a@32'))), '#b(is_int_within_2pow24)')),
    ('fadd', ('ffloor', ('flog2', a)), ('i2f32', b))),
   (('i2f32', ('f2i32', ('ffloor', 'a@32'))), ('ffloor', a)),
]

lower_algebraic.extend(fold_float_int_roundtrip)

lower_scmp = [
   # Float comparisons + bool conversions.
   (('b2f32', ('flt', a, b)), ('slt', a, 'b@32'), '!options->lower_scmp'),
   (('b2f32', ('fge', a, b)), ('sge', a, 'b@32'), '!options->lower_scmp'),
   (('b2f32', ('feq', a, b)), ('seq', a, 'b@32'), '!options->lower_scmp'),
   (('b2f32', ('fneu', a, b)), ('sne', a, 'b@32'), '!options->lower_scmp'),

   # Float comparisons + bool conversions via bcsel.
   (('bcsel@32', ('flt', a, b), 1.0, 0.0), ('slt', a, 'b@32'), '!options->lower_scmp'),
   (('bcsel@32', ('fge', a, b), 1.0, 0.0), ('sge', a, 'b@32'), '!options->lower_scmp'),
   (('bcsel@32', ('feq', a, b), 1.0, 0.0), ('seq', a, 'b@32'), '!options->lower_scmp'),
   (('bcsel@32', ('fneu', a, b), 1.0, 0.0), ('sne', a, 'b@32'), '!options->lower_scmp'),
]

lower_algebraic_late.extend(lower_scmp)

# TODO: core-specific info.
params=[]

def run():
    import nir_algebraic  # pylint: disable=import-error
    print('#include "pco_internal.h"')
    print('#include "nir_search_helpers.h"')
    print('''
static inline bool
is_int_within_2pow24(UNUSED const nir_search_state *state,
                     const nir_alu_instr *instr, unsigned src,
                     unsigned num_components, const uint8_t *swizzle)
{
   if (!nir_src_is_const(instr->src[src].src))
      return false;

   for (unsigned i = 0; i < num_components; i++) {
      int64_t val = nir_src_comp_as_int(instr->src[src].src, swizzle[i]);
      if (val < -(1 << 24) || val > (1 << 24))
         return false;
   }

   return true;
}
''')
    print(nir_algebraic.AlgebraicPass('pco_nir_lower_algebraic', lower_algebraic, params=params).render())
    print(nir_algebraic.AlgebraicPass('pco_nir_lower_algebraic_late', lower_algebraic_late, params=params).render())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--import-path', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.import_path)
    run()

if __name__ == '__main__':
    main()
