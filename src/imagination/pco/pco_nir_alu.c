/*
 * Copyright © 2026 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file pco_nir_alu.c
 *
 * \brief PCO NIR per-ALU instruction pass.
 */
#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/nir/nir_builder_opcodes.h"
#include "compiler/nir/nir_opcodes.h"
#include "compiler/nir/nir_builtin_builder.h"
#include "nir_defines.h"
#include "pco_internal.h"
#include "util/bitset.h"
#include "util/u_dynarray.h"
#include <math.h>

static nir_alu_instr *nan_preserve_minmax(nir_src *src, nir_op op)
{
   if (nir_def_instr_type(src->ssa) != nir_instr_type_alu)
      return NULL;

   nir_alu_instr *minmax = nir_instr_as_alu(nir_def_instr(src->ssa));
   if (minmax->op != op || !nir_alu_instr_is_nan_preserve(minmax))
      return NULL;

   if (!list_is_singular(&minmax->def.uses))
      return NULL;

   return minmax;
}

static nir_alu_instr *splittable_minmax(nir_alu_instr *alu, unsigned *side)
{
   if (alu->op != nir_op_flt && alu->op != nir_op_fge)
      return NULL;

   if (alu->def.num_components != 1)
      return NULL;

   /* flt(fmin(a, b), c) and fge(fmax(a, b), c): the min/max is on the left;
    * flt(c, fmax(a, b)) and fge(c, fmin(a, b)): it is on the right.
    */
   nir_op left_op = alu->op == nir_op_flt ? nir_op_fmin : nir_op_fmax;
   nir_op right_op = alu->op == nir_op_flt ? nir_op_fmax : nir_op_fmin;

   nir_alu_instr *minmax;
   if ((minmax = nan_preserve_minmax(&alu->src[0].src, left_op)))
      *side = 0;
   else if ((minmax = nan_preserve_minmax(&alu->src[1].src, right_op)))
      *side = 1;

   return minmax;
}

static bool minmax_will_split(nir_alu_instr *minmax)
{
   if (!list_is_singular(&minmax->def.uses))
      return false;

   nir_src *use = list_first_entry(&minmax->def.uses, nir_src, use_link);
   if (nir_src_is_if(use))
      return false;

   nir_instr *parent = nir_src_use_instr(use);
   if (parent->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *user = nir_instr_as_alu(parent);

   /* fmin(fmin(a, b), c) feeding a split comparison: the outer split leaves
    * a comparison against the inner fmin on the same side.
    */
   if (user->op == minmax->op && nir_alu_instr_is_nan_preserve(user))
      return minmax_will_split(user);

   unsigned side;
   return splittable_minmax(user, &side) == minmax;
}

/**
 * \brief Splits a comparison against a NaN-preserving fmin/fmax back into
 * two comparisons.
 *
 * nir_opt_algebraic fuses ior(flt(a, c), flt(b, c)) into flt(fmin(a, b), c);
 * the hardware min/max then needs NaN checks on both operands, which costs
 * more than the two comparisons it replaced. Each split here is the exact
 * reverse of one of those ior fusions.
 */
static bool split_minmax_cmp(nir_builder *b, nir_alu_instr *alu)
{
   unsigned side;
   nir_alu_instr *minmax = splittable_minmax(alu, &side);
   if (!minmax)
      return false;

   b->cursor = nir_before_instr(&alu->instr);
   uint32_t old_fp_math_ctrl = b->fp_math_ctrl;
   b->fp_math_ctrl = alu->fp_math_ctrl;

   const unsigned chan = alu->src[side].swizzle[0];
   nir_def *other = nir_ssa_for_alu_src(b, alu, !side);
   nir_def *x =
      nir_channel(b, minmax->src[0].src.ssa, minmax->src[0].swizzle[chan]);
   nir_def *y =
      nir_channel(b, minmax->src[1].src.ssa, minmax->src[1].swizzle[chan]);

   nir_def *cmp_x = side ? nir_build_alu2(b, alu->op, other, x)
                         : nir_build_alu2(b, alu->op, x, other);
   nir_def *cmp_y = side ? nir_build_alu2(b, alu->op, other, y)
                         : nir_build_alu2(b, alu->op, y, other);

   nir_def_replace(&alu->def, nir_ior(b, cmp_x, cmp_y));
   b->fp_math_ctrl = old_fp_math_ctrl;

   return true;
}

/**
 * \brief Lowers ALU instructions with float_controls2 decorations.
 *
 * \param[in,out] b NIR builder.
 * \param[in,out] alu ALU instruction to lower.
 * \param[in,out] cb_data Whether this is the late run.
 * \return True if the pass made progress.
 */
static bool
pco_nir_lower_alu_instr(nir_builder *b, nir_alu_instr *alu, void *cb_data)
{
   const bool late = *(const bool *)cb_data;

   if (late && split_minmax_cmp(b, alu))
      return true;

   if (!nir_alu_instr_is_signed_zero_inf_nan_preserve(alu))
      return false;

   uint32_t old_fp_math_ctrl = b->fp_math_ctrl;
   switch (alu->op) {
   case nir_op_fmax:
   case nir_op_fmin:
      /* All hardware comparison instructions return false if any operand is NaN
       * so a NaN check before is needed.
       * Directly return the other operand if one of the operands is NaN,
       * otherwise return the operation result.
       */
      if (!nir_alu_instr_is_nan_preserve(alu))
         break;

      /* Wait until no more ior fusions can happen, see split_minmax_cmp(). */
      if (!late || minmax_will_split(alu))
         break;

      b->cursor = nir_after_instr(&alu->instr);
      b->fp_math_ctrl = alu->fp_math_ctrl;
      alu->fp_math_ctrl &= ~nir_fp_preserve_nan;

      nir_def *src0_def = nir_ssa_for_alu_src(b, alu, 0);
      nir_def *src1_def = nir_ssa_for_alu_src(b, alu, 1);

      nir_def *fminmax_nan =
         nir_bcsel(b,
                   nir_fisnan(b, src0_def),
                   src1_def,
                   nir_bcsel(b, nir_fisnan(b, src1_def), src0_def, &alu->def));
      nir_def_rewrite_uses_after(&alu->def, fminmax_nan);

      b->fp_math_ctrl = old_fp_math_ctrl;
      return true;

   case nir_op_fsign:
      /* Explicitly check for -0.0 as fsign requires that to be the result in
       * case the input is also -0.0.
       */
      if (!nir_alu_instr_is_signed_zero_preserve(alu))
         break;

      b->fp_math_ctrl = alu->fp_math_ctrl;
      b->cursor = nir_after_instr(&alu->instr);
      nir_def *alu_src = nir_ssa_for_alu_src(b, alu, 0);
      nir_def *fsign_sz = nir_bcsel(b,
                                    nir_feq(b, alu_src, nir_imm_float(b, -0.0)),
                                    nir_imm_float(b, -0.0),
                                    &alu->def);
      alu->fp_math_ctrl &= ~nir_fp_preserve_signed_zero;
      nir_def_rewrite_uses_after(&alu->def, fsign_sz);
      b->fp_math_ctrl = old_fp_math_ctrl;
      return true;

   default:
      break;
   }

   return false;
}

/**
 * \brief Pass that lowers ALU instructions based on special conditions.
 *
 * \param[in,out] shader NIR shader.
 * \return True if the pass made progress.
 */
bool pco_nir_lower_alu(nir_shader *shader, bool late)
{
   return nir_shader_alu_pass(shader,
                              pco_nir_lower_alu_instr,
                              nir_metadata_control_flow,
                              &late);
}

struct trig_use {
   nir_alu_instr *trig;
   double c;
};

/* Returns the non-constant part of angle = ffma(a, b, #c) or fadd(y, #c). */
static nir_alu_instr *trig_angle(nir_alu_instr *trig, double *c)
{
   if (trig->def.num_components != 1 || trig->def.bit_size != 32)
      return NULL;

   nir_def *src = trig->src[0].src.ssa;
   if (nir_def_instr_type(src) != nir_instr_type_alu)
      return NULL;

   nir_alu_instr *angle = nir_instr_as_alu(nir_def_instr(src));
   if (angle->def.num_components != 1)
      return NULL;

   /* Splitting ffma(a, b, c) rounds a * b on its own: only allowed when
    * the source did not ask for an exact (non-contracted) evaluation.
    */
   if (nir_alu_instr_is_exact(angle) || nir_alu_instr_is_exact(trig))
      return NULL;

   unsigned const_src;
   if (angle->op == nir_op_ffma)
      const_src = 2;
   else if (angle->op == nir_op_fadd)
      const_src = nir_src_is_const(angle->src[1].src) ? 1 : 0;
   else
      return NULL;

   if (!nir_src_is_const(angle->src[const_src].src))
      return NULL;

   for (unsigned s = 0; s < nir_op_infos[angle->op].num_inputs; ++s) {
      if (s != const_src && nir_src_is_const(angle->src[s].src))
         return NULL;
   }

   *c = nir_src_comp_as_float(angle->src[const_src].src,
                              angle->src[const_src].swizzle[0]);
   return angle;
}

/* Whether two angles have the same non-constant part. */
static bool same_trig_base(nir_alu_instr *a, nir_alu_instr *b)
{
   if (a->op != b->op)
      return false;

   unsigned n = nir_op_infos[a->op].num_inputs;
   for (unsigned s = 0; s < n; ++s) {
      bool a_const = nir_src_is_const(a->src[s].src);
      if (a_const != nir_src_is_const(b->src[s].src))
         return false;

      if (a_const)
         continue;

      if (a->src[s].src.ssa != b->src[s].src.ssa ||
          a->src[s].swizzle[0] != b->src[s].swizzle[0])
         return false;
   }

   return true;
}

static nir_def *trig_base(nir_builder *b, nir_alu_instr *angle)
{
   if (angle->op == nir_op_ffma)
      return nir_fmul(b,
                      nir_ssa_for_alu_src(b, angle, 0),
                      nir_ssa_for_alu_src(b, angle, 1));

   unsigned var_src = nir_src_is_const(angle->src[1].src) ? 0 : 1;
   return nir_ssa_for_alu_src(b, angle, var_src);
}

/**
 * \brief Shares sin/cos between angles that only differ by a constant.
 *
 * Each sin or cos costs a range reduction and a fsinc. With angles x + c_i,
 * such as the spiral taps of an ambient occlusion kernel, sin(x) and cos(x)
 * are computed once and every sin(x + c_i) / cos(x + c_i) becomes two
 * multiply-adds with the constant sin(c_i) and cos(c_i).
 *
 * \param[in,out] shader NIR shader.
 * \return True if the pass made progress.
 */
bool pco_nir_expand_trig(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function_impl (impl, shader) {
      nir_builder b = nir_builder_create(impl);

      nir_foreach_block (block, impl) {
         struct util_dynarray trigs;
         util_dynarray_init(&trigs, NULL);

         nir_foreach_instr (instr, block) {
            if (instr->type != nir_instr_type_alu)
               continue;

            nir_alu_instr *alu = nir_instr_as_alu(instr);
            if (alu->op != nir_op_fsin && alu->op != nir_op_fcos)
               continue;

            double c;
            if (trig_angle(alu, &c))
               util_dynarray_append(&trigs, ((struct trig_use){ alu, c }));
         }

         unsigned count = util_dynarray_num_elements(&trigs, struct trig_use);
         struct trig_use *uses = util_dynarray_begin(&trigs);
         BITSET_WORD *done = rzalloc_array(NULL, BITSET_WORD, BITSET_WORDS(count));

         for (unsigned i = 0; i < count; ++i) {
            if (BITSET_TEST(done, i))
               continue;

            double ci;
            nir_alu_instr *angle = trig_angle(uses[i].trig, &ci);

            unsigned group = 0;
            for (unsigned j = i; j < count; ++j) {
               double cj;
               if (!BITSET_TEST(done, j) &&
                   same_trig_base(angle, trig_angle(uses[j].trig, &cj)))
                  ++group;
            }

            /* Two sin/cos are needed for the base anyway. */
            if (group < 3)
               continue;

            b.cursor = nir_before_instr(&uses[i].trig->instr);
            b.fp_math_ctrl = uses[i].trig->fp_math_ctrl;
            nir_def *base = trig_base(&b, angle);
            nir_def *s = nir_fsin(&b, base);
            nir_def *k = nir_fcos(&b, base);

            for (unsigned j = i; j < count; ++j) {
               double cj;
               nir_alu_instr *trig = uses[j].trig;
               if (BITSET_TEST(done, j) ||
                   !same_trig_base(angle, trig_angle(trig, &cj)))
                  continue;

               BITSET_SET(done, j);

               b.cursor = nir_before_instr(&trig->instr);
               nir_def *sc = nir_imm_float(&b, sin(cj));
               nir_def *cc = nir_imm_float(&b, cos(cj));

               /* sin(x + c) = sin x cos c + cos x sin c
                * cos(x + c) = cos x cos c - sin x sin c
                */
               nir_def *res =
                  trig->op == nir_op_fsin
                     ? nir_ffma(&b, s, cc, nir_fmul(&b, k, sc))
                     : nir_ffma(&b, k, cc, nir_fneg(&b, nir_fmul(&b, s, sc)));

               nir_def_replace(&trig->def, res);
               progress = true;
            }
         }

         ralloc_free(done);
         util_dynarray_fini(&trigs);
      }

      nir_progress(progress, impl, nir_metadata_control_flow);
   }

   return progress;
}
