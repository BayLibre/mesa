/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file pco_group_instrs.c
 *
 * \brief PCO instruction grouping pass.
 */

#include "hwdef/rogue_hw_defs.h"
#include "pco.h"
#include "pco_builder.h"
#include "pco_map.h"
#include "util/macros.h"

#include <stdbool.h>

/**
 * \brief Calculates the decode-assist value for an instruction group.
 *
 * \param[in] igrp PCO instruction group.
 * \return The decode-assist value.
 */
static inline unsigned calc_da(pco_igrp *igrp)
{
   unsigned da = igrp->enc.len.hdr;
   bool no_srcs_dests = !igrp->enc.len.lower_srcs &&
                        !igrp->enc.len.upper_srcs && !igrp->enc.len.dests;

   switch (igrp->hdr.alutype) {
   case PCO_ALUTYPE_MAIN:
   case PCO_ALUTYPE_BITWISE: {
      pco_foreach_phase_rev (p) {
         if (igrp->hdr.alutype == PCO_ALUTYPE_BITWISE || p > PCO_OP_PHASE_1)
            da += igrp->enc.len.instrs[p];
      }
      break;
   }

   case PCO_ALUTYPE_CONTROL:
      if (no_srcs_dests)
         return 0;

      da += igrp->enc.len.instrs[PCO_OP_PHASE_CTRL];
      break;

   default:
      UNREACHABLE("");
   }

   return da;
}

/**
 * \brief Calculates the lengths for an instruction group.
 *
 * \param[in,out] igrp PCO instruction group.
 * \param[in,out] offset_bytes The cumulative shader offset (in bytes).
 */
static inline void calc_lengths(pco_igrp *igrp, unsigned *offset_bytes)
{
   unsigned total_length = 0;

   igrp->enc.len.hdr = pco_igrp_hdr_bytes(igrp->variant.hdr);
   total_length += igrp->enc.len.hdr;

   igrp->enc.len.lower_srcs = pco_src_bytes(igrp->variant.lower_src);
   total_length += igrp->enc.len.lower_srcs;

   igrp->enc.len.upper_srcs = pco_src_bytes(igrp->variant.upper_src);
   total_length += igrp->enc.len.upper_srcs;

   igrp->enc.len.iss = pco_iss_bytes(igrp->variant.iss);
   total_length += igrp->enc.len.iss;

   igrp->enc.len.dests = pco_dst_bytes(igrp->variant.dest);
   total_length += igrp->enc.len.dests;

   pco_foreach_phase_in_igrp (igrp, phase) {
      switch (igrp->hdr.alutype) {
      case PCO_ALUTYPE_MAIN:
         if (phase == PCO_OP_PHASE_BACKEND) {
            igrp->enc.len.instrs[phase] =
               pco_backend_bytes(igrp->variant.instr[phase].backend);
         } else {
            igrp->enc.len.instrs[phase] =
               pco_main_bytes(igrp->variant.instr[phase].main);
         }
         break;

      case PCO_ALUTYPE_BITWISE:
         igrp->enc.len.instrs[phase] =
            pco_bitwise_bytes(igrp->variant.instr[phase].bitwise);
         break;

      case PCO_ALUTYPE_CONTROL:
         igrp->enc.len.instrs[phase] =
            pco_ctrl_bytes(igrp->variant.instr[phase].control);
         break;

      default:
         UNREACHABLE("");
      }

      total_length += igrp->enc.len.instrs[phase];
   }

   igrp->enc.len.word_padding = total_length % 2;
   total_length += igrp->enc.len.word_padding;

   igrp->enc.len.total = total_length;

   /* Set igrp header length and decode-assist. */
   igrp->hdr.length = igrp->enc.len.total / 2;
   igrp->hdr.da = calc_da(igrp);

   /* Set offset and update running offset byte count. */
   igrp->enc.offset = *offset_bytes;
   *offset_bytes += igrp->enc.len.total;
}

/**
 * \brief Calculates the alignment padding to be applied to
 *        the last instruction group in the shader.
 *
 * \param[in,out] last_igrp The last instruction group.
 * \param[in,out] offset_bytes The cumulative shader offset (in bytes).
 */
static inline void calc_align_padding(pco_igrp *last_igrp,
                                      unsigned *offset_bytes)
{
   /* We should never end up with a completely empty shader. */
   assert(last_igrp);

   unsigned total_align = last_igrp->enc.len.total % ROGUE_ICACHE_ALIGN;
   unsigned offset_align = last_igrp->enc.offset % ROGUE_ICACHE_ALIGN;

   if (total_align) {
      unsigned padding = ROGUE_ICACHE_ALIGN - total_align;
      *offset_bytes += padding;

      /* Pad the size of the last igrp. */
      last_igrp->enc.len.align_padding += padding;
      last_igrp->enc.len.total += padding;

      /* Update the last igrp header length. */
      last_igrp->hdr.length = last_igrp->enc.len.total / 2;
   }

   if (offset_align) {
      unsigned padding = ROGUE_ICACHE_ALIGN - offset_align;
      *offset_bytes += padding;

      /* Pad the size of the penultimate igrp. */
      pco_igrp *penultimate_igrp = pco_prev_igrp(last_igrp);

      /* If we only have one igrp then its offset will be zero. */
      assert(penultimate_igrp);

      penultimate_igrp->enc.len.align_padding += padding;
      penultimate_igrp->enc.len.total += padding;

      /* Update the penultimate igrp header length. */
      penultimate_igrp->hdr.length = penultimate_igrp->enc.len.total / 2;

      /* Update the offset of the last igrp. */
      last_igrp->enc.offset += padding;
   }
}

/* How far ahead to look for an instruction to co-issue in phase 1. */
#define COISSUE_WINDOW 16

/**
 * \brief Returns whether an op has a phase 0 and phase 1 encoding with
 *        identical semantics (FT0 = op(S0..S2) / FT1 = op(S3..S5)).
 */
static bool coissue_op(enum pco_op op)
{
   switch (op) {
   case PCO_OP_FADD:
   case PCO_OP_FMUL:
   case PCO_OP_FMAD:
   case PCO_OP_MBYP:
      return true;

   default:
      break;
   }

   return false;
}

static bool ref_is_io(pco_ref ref, enum pco_io io)
{
   return pco_ref_is_io(ref) && pco_ref_get_io(ref) == io;
}

/**
 * \brief Returns whether an igrp is a lone phase 0 op that can either host a
 *        phase 1 op or be moved into phase 1 of another igrp.
 *
 * Such an igrp only reads S0..S2, routes FT0 to W0 through IS4 and writes a
 * single temp register.
 */
static bool igrp_is_coissue_candidate(const pco_igrp *igrp)
{
   if (igrp->hdr.alutype != PCO_ALUTYPE_MAIN ||
       igrp->hdr.oporg != PCO_OPORG_P0)
      return false;

   if (igrp->hdr.olchk || igrp->hdr.end || igrp->hdr.atom ||
       igrp->hdr.rpt > 1)
      return false;

   pco_foreach_phase (p) {
      if (p != PCO_OP_PHASE_0 && igrp->instrs[p])
         return false;
   }

   pco_instr *instr = igrp->instrs[PCO_OP_PHASE_0];
   if (!instr || !coissue_op(instr->op))
      return false;

   for (unsigned u = ROGUE_ALU_INPUT_GROUP_SIZE; u < ROGUE_MAX_ALU_INPUTS; ++u) {
      if (!pco_ref_is_null(igrp->srcs.s[u]))
         return false;
   }

   for (unsigned u = 0; u < ROGUE_MAX_ALU_INTERNAL_SOURCES; ++u) {
      if (u == 4) {
         if (!ref_is_io(igrp->iss.is[u], PCO_IO_FT0))
            return false;
      } else if (!pco_ref_is_null(igrp->iss.is[u])) {
         return false;
      }
   }

   if (!pco_ref_is_null(igrp->dests.w[1]) || !igrp->hdr.w0p || igrp->hdr.w1p)
      return false;

   pco_ref w0 = igrp->dests.w[0];
   return pco_ref_is_reg(w0) && pco_ref_get_reg_class(w0) == PCO_REG_CLASS_TEMP;
}

/**
 * \brief Returns whether two hardware register references may alias.
 */
static bool refs_may_alias(pco_ref a, pco_ref b)
{
   bool a_hw = pco_ref_is_reg(a) || pco_ref_is_idx_reg(a);
   bool b_hw = pco_ref_is_reg(b) || pco_ref_is_idx_reg(b);
   if (!a_hw || !b_hw)
      return false;

   if (pco_ref_is_idx_reg(a) || pco_ref_is_idx_reg(b))
      return true;

   enum pco_reg_class a_class = pco_ref_get_reg_class(a);
   enum pco_reg_class b_class = pco_ref_get_reg_class(b);

   switch (a_class) {
   case PCO_REG_CLASS_TEMP:
   case PCO_REG_CLASS_VTXIN:
   case PCO_REG_CLASS_COEFF:
   case PCO_REG_CLASS_SHARED:
      if (a_class != b_class)
         return false;

      return a.val < b.val + pco_ref_get_chans(b) &&
             b.val < a.val + pco_ref_get_chans(a);

   default:
      break;
   }

   /* The remaining classes all live in the special register bank. */
   switch (b_class) {
   case PCO_REG_CLASS_TEMP:
   case PCO_REG_CLASS_VTXIN:
   case PCO_REG_CLASS_COEFF:
   case PCO_REG_CLASS_SHARED:
      return false;

   default:
      break;
   }

   return true;
}

static bool igrp_reads(const pco_igrp *igrp, pco_ref ref)
{
   for (unsigned u = 0; u < ROGUE_MAX_ALU_INPUTS; ++u) {
      if (refs_may_alias(igrp->srcs.s[u], ref))
         return true;
   }

   return false;
}

static bool igrp_writes(const pco_igrp *igrp, pco_ref ref)
{
   for (unsigned u = 0; u < ROGUE_MAX_ALU_OUTPUTS; ++u) {
      if (refs_may_alias(igrp->dests.w[u], ref))
         return true;
   }

   return false;
}

/**
 * \brief Returns whether no igrp may be moved across igrp i.
 *
 * Only plain ALU igrps, whose register accesses are all described by their
 * S and W ports, can be reordered.
 */
static bool igrp_is_barrier(const pco_igrp *i)
{
   if (i->hdr.alutype != PCO_ALUTYPE_MAIN &&
       i->hdr.alutype != PCO_ALUTYPE_BITWISE)
      return true;

   return i->instrs[PCO_OP_PHASE_BACKEND] || i->hdr.olchk || i->hdr.atom ||
          i->hdr.end || i->hdr.rpt > 1;
}

/**
 * \brief Returns whether igrp b can be moved before igrp i.
 */
static bool igrp_can_hoist_over(const pco_igrp *b, const pco_igrp *i)
{
   if (igrp_is_barrier(i))
      return false;

   for (unsigned u = 0; u < ROGUE_MAX_ALU_OUTPUTS; ++u) {
      if (igrp_writes(b, i->dests.w[u]) || igrp_reads(b, i->dests.w[u]))
         return false;
   }

   for (unsigned u = 0; u < ROGUE_MAX_ALU_INPUTS; ++u) {
      if (igrp_writes(b, i->srcs.s[u]))
         return false;
   }

   return true;
}

/**
 * \brief Returns whether igrp b can be co-issued in phase 1 of igrp a.
 *
 * Sources are read before any destination of the group is written, so b may
 * overwrite a register that a reads, but it may neither read nor write the
 * destination of a.
 */
static bool igrps_can_pair(const pco_igrp *a, const pco_igrp *b)
{
   if (a->hdr.cc != b->hdr.cc)
      return false;

   if (igrp_reads(b, a->dests.w[0]) || igrp_writes(b, a->dests.w[0]))
      return false;

   /* b's sources move from S0..S2 to S3..S5. */
   for (unsigned u = 0; u < ROGUE_ALU_INPUT_GROUP_SIZE; ++u) {
      if (!ref_src_map_valid(b->srcs.s[u], PCO_IO_S3 + u, NULL))
         return false;
   }

   pco_igrp tmp = *a;
   for (unsigned u = 0; u < ROGUE_ALU_INPUT_GROUP_SIZE; ++u)
      tmp.srcs.s[ROGUE_ALU_INPUT_GROUP_SIZE + u] = b->srcs.s[u];
   tmp.dests.w[1] = b->dests.w[0];

   return pco_igrp_src_variant_try(&tmp, true) >= 0 &&
          pco_igrp_dest_variant_try(&tmp) >= 0;
}

/**
 * \brief Moves the phase 0 op of igrp b into phase 1 of igrp a and deletes b.
 */
static void igrp_pair(pco_igrp *a, pco_igrp *b)
{
   pco_instr *instr = b->instrs[PCO_OP_PHASE_0];

   pco_foreach_instr_src (psrc, instr) {
      if (!pco_ref_is_io(*psrc))
         continue;

      enum pco_io io = pco_ref_get_io(*psrc);
      assert(io >= PCO_IO_S0 && io <= PCO_IO_S2);
      psrc->val = io - PCO_IO_S0 + PCO_IO_S3;
   }

   pco_foreach_instr_dest (pdest, instr) {
      assert(ref_is_io(*pdest, PCO_IO_FT0));
      pdest->val = PCO_IO_FT1;
   }

   instr->phase = PCO_OP_PHASE_1;
   instr->parent_igrp = a;
   ralloc_steal(a, instr);
   a->instrs[PCO_OP_PHASE_1] = instr;
   a->variant.instr[PCO_OP_PHASE_1].main =
      b->variant.instr[PCO_OP_PHASE_0].main;

   for (unsigned u = 0; u < ROGUE_ALU_INPUT_GROUP_SIZE; ++u)
      a->srcs.s[ROGUE_ALU_INPUT_GROUP_SIZE + u] = b->srcs.s[u];

   a->iss.is[5] = pco_ref_io(PCO_IO_FT1);
   a->dests.w[1] = b->dests.w[0];

   a->hdr.oporg = PCO_OPORG_P0_P1;
   a->hdr.w1p = true;

   a->variant.upper_src = pco_igrp_src_variant(a, true);
   a->variant.dest = pco_igrp_dest_variant(a);

   list_del(&b->link);
   ralloc_free(b);
}

/**
 * \brief Co-issues independent lone phase 0 ops of a block in pairs, the
 *        later one being hoisted into phase 1 of the earlier one.
 *
 * \param[in,out] block PCO block.
 */
static void coissue_block(pco_block *block)
{
   pco_foreach_igrp_in_block (a, block) {
      /* The igrp after a control op may be the target of a skip branch. */
      pco_igrp *prev = pco_prev_igrp(a);
      if (prev && prev->hdr.alutype == PCO_ALUTYPE_CONTROL)
         continue;

      if (!igrp_is_coissue_candidate(a))
         continue;

      unsigned window = a->hdr.cc == PCO_CC_E1_ZX ? COISSUE_WINDOW : 1;
      pco_igrp *b = a;
      for (unsigned dist = 0; dist < window; ++dist) {
         if (b == pco_last_igrp(block))
            break;

         pco_igrp *i = b;
         b = list_entry(b->link.next, pco_igrp, link);

         if (i != a && igrp_is_barrier(i))
            break;

         if (!igrp_is_coissue_candidate(b))
            continue;

         if (igrps_can_pair(a, b)) {
            bool ok = true;
            for (pco_igrp *j = list_entry(a->link.next, pco_igrp, link);
                 j != b;
                 j = list_entry(j->link.next, pco_igrp, link)) {
               if (!igrp_can_hoist_over(b, j)) {
                  ok = false;
                  break;
               }
            }

            if (ok) {
               igrp_pair(a, b);
               break;
            }
         }
      }
   }
}

/**
 * \brief Groups PCO instructions into instruction groups.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
bool pco_group_instrs(pco_shader *shader)
{
   pco_builder b;
   pco_igrp *igrp = NULL;
   unsigned offset_bytes = 0;

   assert(!shader->is_grouped);

   pco_foreach_func_in_shader (func, shader) {
      pco_foreach_block_in_func (block, func) {
         b = pco_builder_create(func, pco_cursor_before_block(block));
         pco_foreach_instr_in_block_safe (instr, block) {
            igrp = pco_igrp_create(func);
            pco_map_igrp(igrp, instr);
            pco_builder_insert_igrp(&b, igrp);
         }

         if (!PCO_DEBUG(NO_COISSUE))
            coissue_block(block);
      }

      /* TODO: double check that *start* alignment is satisfied by
       * calc_align_padding when having multiple functions?
       */
      func->next_igrp = 0;
      pco_foreach_igrp_in_func (cur, func) {
         cur->index = func->next_igrp++;
         calc_lengths(cur, &offset_bytes);
         igrp = cur;
      }

      /* Ensure the final instruction group has a total size and offset
       * that are a multiple of the icache alignment.
       */
      calc_align_padding(igrp, &offset_bytes);
   }

   shader->is_grouped = true;
   return true;
}
