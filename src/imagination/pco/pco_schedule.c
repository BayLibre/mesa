/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file pco_schedule.c
 *
 * \brief PCO instruction scheduling pass.
 */

#include "pco.h"
#include "pco_builder.h"
#include "pco_internal.h"
#include "util/hash_table.h"
#include "util/macros.h"
#include "util/u_dynarray.h"

#include <inttypes.h>
#include <stdbool.h>

/**
 * \brief Schedules instructions and inserts waits.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
bool pco_schedule(pco_shader *shader)
{
   bool progress = false;
   pco_builder b;

   pco_foreach_func_in_shader (func, shader) {
      pco_foreach_instr_in_func_safe (instr, func) {
         if (instr->op == PCO_OP_WDF || instr->op == PCO_OP_IDF)
            continue;

         pco_foreach_instr_src (psrc, instr) {
            if (!pco_ref_is_drc(*psrc))
               continue;

            b = pco_builder_create(func, pco_cursor_after_instr(instr));

            if ((instr->op == PCO_OP_ST32 || instr->op == PCO_OP_ST32_REGBL) &&
                pco_instr_get_idf(instr)) {
               pco_ref addr = pco_ref_chans(instr->src[3], 2);
               pco_idf(&b, *psrc, addr);
               pco_instr_set_idf(instr, false);
            }

            pco_wdf(&b, *psrc);

            progress = true;
            break;
         }
      }
   }

   return progress;
}

/** Registers written or read by a data fence operation still in flight. */
struct pending_range {
   pco_ref drc;
   enum pco_reg_class class;
   unsigned start;
   unsigned end;
   bool written; /* Destination: any later access is a hazard. */
};

static bool ref_range(pco_ref ref,
                      unsigned rpt,
                      enum pco_reg_class *class,
                      unsigned *start,
                      unsigned *end)
{
   if (!pco_ref_is_reg(ref))
      return false;

   *class = pco_ref_get_reg_class(ref);
   *start = ref.val;
   *end = ref.val + MAX2(pco_ref_get_chans(ref), 1) * MAX2(rpt, 1);
   return true;
}

static unsigned instr_rpt(pco_instr *instr)
{
   return pco_instr_has_rpt(instr) ? pco_instr_get_rpt(instr) : 1;
}

static void wait_drc_ranges(pco_func *func,
                            pco_cursor cursor,
                            struct util_dynarray *pending,
                            pco_ref drc)
{
   pco_builder b = pco_builder_create(func, cursor);
   pco_wdf(&b, drc);

   unsigned kept = 0;
   util_dynarray_foreach (pending, struct pending_range, range) {
      if (range->drc.val == drc.val)
         continue;

      *util_dynarray_element(pending, struct pending_range, kept++) = *range;
   }
   pending->size = kept * sizeof(struct pending_range);
}

static void wait_all_ranges(pco_func *func,
                            pco_cursor cursor,
                            struct util_dynarray *pending)
{
   while (util_dynarray_num_elements(pending, struct pending_range)) {
      pco_ref drc =
         util_dynarray_element(pending, struct pending_range, 0)->drc;
      wait_drc_ranges(func, cursor, pending, drc);
   }
}

/**
 * \brief Returns the data fence counter whose in-flight operations an access
 * conflicts with, if any.
 */
static bool access_conflicts(struct util_dynarray *pending,
                             pco_ref ref,
                             unsigned rpt,
                             bool write,
                             pco_ref *drc)
{
   if (pco_ref_is_idx_reg(ref)) {
      /* Unknown registers: wait for everything. */
      if (!util_dynarray_num_elements(pending, struct pending_range))
         return false;

      *drc = util_dynarray_element(pending, struct pending_range, 0)->drc;
      return true;
   }

   enum pco_reg_class class;
   unsigned start, end;
   if (!ref_range(ref, rpt, &class, &start, &end))
      return false;

   util_dynarray_foreach (pending, struct pending_range, range) {
      if (!range->written && !write)
         continue;

      if (range->class == class && start < range->end && range->start < end) {
         *drc = range->drc;
         return true;
      }
   }

   return false;
}

static bool is_deferrable(const pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_SMP:
   case PCO_OP_LD:
   case PCO_OP_LD_REGBL:
      return true;

   default:
      return false;
   }
}

static bool ends_straight_line(pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_BR:
   case PCO_OP_BREAK:
   case PCO_OP_CONTINUE:
      return true;

   default:
      break;
   }

   return pco_instr_has_end(instr) && pco_instr_get_end(instr);
}

/**
 * \brief Moves the wait after a sample or load down to the first access to
 * the registers it uses.
 *
 * Runs after register allocation, so that physical registers, including the
 * unused components of a destination, are protected until the wait: a late
 * result must not land in a register already reused for something else.
 * Everything is waited for before control flow and at the end of each block.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
bool pco_defer_waits(pco_shader *shader)
{
   bool progress = false;
   struct util_dynarray pending;
   util_dynarray_init(&pending, NULL);

   pco_foreach_func_in_shader (func, shader) {
      pco_foreach_block_in_func (block, func) {
         pco_instr *skip_wdf = NULL;

         pco_foreach_instr_in_block_safe (instr, block) {
            if (instr == skip_wdf) {
               pco_instr_delete(instr);
               skip_wdf = NULL;
               progress = true;
               continue;
            }
            skip_wdf = NULL;

            if (instr->op == PCO_OP_WDF) {
               wait_drc_ranges(func,
                               pco_cursor_before_instr(instr),
                               &pending,
                               instr->src[0]);
               pco_instr_delete(instr);
               continue;
            }

            pco_ref drc_src = pco_ref_null();
            pco_foreach_instr_src (psrc, instr) {
               if (pco_ref_is_drc(*psrc))
                  drc_src = *psrc;
            }
            bool uses_drc = !pco_ref_is_null(drc_src);

            if (ends_straight_line(instr) || instr->op == PCO_OP_IDF ||
                (uses_drc && !is_deferrable(instr))) {
               wait_all_ranges(func, pco_cursor_before_instr(instr), &pending);
               continue;
            }

            unsigned rpt = instr_rpt(instr);
            pco_ref drc;
            bool waited;
            do {
               waited = false;
               pco_foreach_instr_src (psrc, instr) {
                  if (access_conflicts(&pending, *psrc, rpt, false, &drc)) {
                     wait_drc_ranges(func,
                                     pco_cursor_before_instr(instr),
                                     &pending,
                                     drc);
                     waited = true;
                     break;
                  }
               }
               if (waited)
                  continue;

               pco_foreach_instr_dest (pdest, instr) {
                  if (access_conflicts(&pending, *pdest, rpt, true, &drc)) {
                     wait_drc_ranges(func,
                                     pco_cursor_before_instr(instr),
                                     &pending,
                                     drc);
                     waited = true;
                     break;
                  }
               }
            } while (waited);

            if (!uses_drc)
               continue;

            /* A deferrable operation: its wait is the next instruction. */
            pco_instr *next = pco_next_instr(instr);
            if (!next || next->op != PCO_OP_WDF ||
                next->src[0].val != drc_src.val)
               continue;

            pco_foreach_instr_dest (pdest, instr) {
               struct pending_range range = { .drc = drc_src, .written = true };
               if (ref_range(*pdest, rpt, &range.class, &range.start, &range.end))
                  util_dynarray_append(&pending, range);
            }
            pco_foreach_instr_src (psrc, instr) {
               struct pending_range range = { .drc = drc_src, .written = false };
               if (ref_range(*psrc, rpt, &range.class, &range.start, &range.end))
                  util_dynarray_append(&pending, range);
            }

            skip_wdf = next;
         }

         pco_instr *last = pco_last_instr(block);
         wait_all_ranges(func,
                         last && ends_straight_line(last)
                            ? pco_cursor_before_instr(last)
                            : pco_cursor_after_block(block),
                         &pending);
      }
   }

   util_dynarray_fini(&pending);

   return progress;
}

/** How far back an immediate load can be reused, to bound register pressure. */
#define PCO_IMM_REUSE_WINDOW 32

struct imm_def {
   uint32_t val;
   unsigned ssa;
   unsigned pos;
};

/* Rewrites the uses of every replaced SSA value in one walk of the function. */
static void rewrite_ssa_uses(pco_func *func, struct hash_table_u64 *repl)
{
   pco_foreach_instr_in_func (instr, func) {
      pco_foreach_instr_src_ssa (psrc, instr) {
         void *to = _mesa_hash_table_u64_search(repl, psrc->val);
         if (to)
            psrc->val = (uintptr_t)to - 1;
      }
   }
}

/**
 * \brief Reuses an earlier load of the same immediate in the block instead of
 * loading it again for each instruction that uses it.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
bool pco_reuse_imms(pco_shader *shader)
{
   bool progress = false;

   pco_foreach_func_in_shader (func, shader) {
      struct hash_table_u64 *repl = _mesa_hash_table_u64_create(NULL);

      pco_foreach_block_in_func (block, func) {
         /* Last load of each immediate value in the block. */
         struct hash_table_u64 *defs = _mesa_hash_table_u64_create(NULL);
         unsigned pos = 0;

         pco_foreach_instr_in_block_safe (instr, block) {
            ++pos;

            if (instr->op != PCO_OP_MOVI32 || !pco_ref_is_ssa(instr->dest[0]))
               continue;

            if (pco_instr_has_exec_cnd(instr) &&
                pco_instr_get_exec_cnd(instr) != PCO_EXEC_CND_E1_ZX)
               continue;

            uint32_t val = pco_ref_get_imm(instr->src[0]);
            struct imm_def *found = _mesa_hash_table_u64_search(defs, val);

            if (found && pos - found->pos <= PCO_IMM_REUSE_WINDOW) {
               _mesa_hash_table_u64_insert(repl,
                                           instr->dest[0].val,
                                           (void *)(uintptr_t)(found->ssa + 1));
               found->pos = pos;
               pco_instr_delete(instr);
               progress = true;
               continue;
            }

            if (!found) {
               found = ralloc(defs, struct imm_def);
               _mesa_hash_table_u64_insert(defs, val, found);
            }

            *found = (struct imm_def){
               .val = val,
               .ssa = instr->dest[0].val,
               .pos = pos,
            };
         }

         _mesa_hash_table_u64_destroy(defs);
      }

      rewrite_ssa_uses(func, repl);
      _mesa_hash_table_u64_destroy(repl);
   }

   return progress;
}

/** Whether a hardware register keeps its value for the whole shader. */
static bool is_invariant_hwreg(const pco_shader *shader, pco_ref ref)
{
   if (!pco_ref_is_reg(ref))
      return false;

   switch (pco_ref_get_reg_class(ref)) {
   case PCO_REG_CLASS_SPEC:
   case PCO_REG_CLASS_CONST:
      return true;

   case PCO_REG_CLASS_SHARED:
   case PCO_REG_CLASS_COEFF:
      return shader->stage != MESA_SHADER_COMPUTE;

   default:
      return false;
   }
}

/**
 * \brief Reuses an earlier vec of the same block built from the same values,
 *        where an immediate load counts as its value.
 *
 * Every texture sample builds its own coordinate vec, even when several
 * samples read the same coordinates, which then costs one copy per
 * component and sample.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
bool pco_cse_vecs(pco_shader *shader)
{
   bool progress = false;

   pco_foreach_func_in_shader (func, shader) {
      void *mem_ctx = ralloc_context(NULL);
      struct hash_table_u64 *repl = _mesa_hash_table_u64_create(mem_ctx);

      /* Immediate value + 1 of each SSA value loaded by a movi32. */
      uint64_t *imm_of = rzalloc_array(mem_ctx, uint64_t, func->next_ssa);
      pco_foreach_instr_in_func (instr, func) {
         if (instr->op != PCO_OP_MOVI32 || !pco_ref_is_ssa(instr->dest[0]))
            continue;

         if (pco_instr_has_exec_cnd(instr) &&
             pco_instr_get_exec_cnd(instr) != PCO_EXEC_CND_E1_ZX)
            continue;

         imm_of[instr->dest[0].val] =
            (uint64_t)pco_ref_get_imm(instr->src[0]) + 1;
      }

      pco_foreach_block_in_func (block, func) {
         struct hash_table *vecs = _mesa_hash_table_create(mem_ctx,
                                                           _mesa_hash_string,
                                                           _mesa_key_string_equal);

         pco_foreach_instr_in_block_safe (instr, block) {
            if (instr->op != PCO_OP_VEC || !pco_ref_is_ssa(instr->dest[0]))
               continue;

            if (pco_instr_has_exec_cnd(instr) &&
                pco_instr_get_exec_cnd(instr) != PCO_EXEC_CND_E1_ZX)
               continue;

            char *key = ralloc_asprintf(mem_ctx,
                                        "%u",
                                        pco_ref_get_bits(instr->dest[0]));
            bool keyed = true;
            pco_foreach_instr_src (psrc, instr) {
               if (pco_ref_has_mods_set(*psrc)) {
                  keyed = false;
                  break;
               }

               if (is_invariant_hwreg(shader, *psrc)) {
                  ralloc_asprintf_append(&key,
                                         ",r%u:%u:%u",
                                         pco_ref_get_reg_class(*psrc),
                                         pco_ref_get_reg_index(*psrc),
                                         pco_ref_get_chans(*psrc));
                  continue;
               }

               if (!pco_ref_is_ssa(*psrc)) {
                  keyed = false;
                  break;
               }

               uint64_t imm = imm_of[psrc->val];
               if (imm)
                  ralloc_asprintf_append(&key, ",i%" PRIu64, imm - 1);
               else
                  ralloc_asprintf_append(&key, ",s%u", psrc->val);
            }

            if (!keyed)
               continue;

            struct hash_entry *entry = _mesa_hash_table_search(vecs, key);
            if (!entry) {
               _mesa_hash_table_insert(vecs, key, instr);
               continue;
            }

            pco_instr *earlier = entry->data;
            _mesa_hash_table_u64_insert(repl,
                                        instr->dest[0].val,
                                        (void *)(uintptr_t)(earlier->dest[0].val + 1));
            pco_instr_delete(instr);
            progress = true;
         }
      }

      if (progress)
         rewrite_ssa_uses(func, repl);

      ralloc_free(mem_ctx);
   }

   return progress;
}

/* Pre-RA list scheduling of straight-line ALU code. */

/** How far ahead of the original order an instruction can be pulled up. */
#define SCHED_WINDOW 16

/**
 * Neighbourhood whose original pressure bounds the pressure of a position
 * when regions span non-ALU instructions. Regions made of ALU code only are
 * bounded by their overall original peak instead.
 */
#define SCHED_CAP_WINDOW 4

/** How far ahead the next pinned run can be pulled up. */
#define SCHED_PIN_WINDOW (2 * SCHED_WINDOW)

/** Scheduling DAG node: one instruction of a region. */
struct sched_node {
   pco_instr *instr;
   unsigned orig; /** Position in the original order. */
   unsigned num_preds; /** Unscheduled predecessors. */
   struct util_dynarray succs; /** struct sched_node * */
   struct util_dynarray preds; /** struct sched_node * */
   unsigned visit; /** Last ancestor walk that reached the node. */
   int defs; /** Register units of the dests that are used. */
   bool pinned; /** Keeps its order relative to the other pinned nodes. */
   bool scheduled;
};

struct sched_ssa {
   unsigned region; /** Region the fields below belong to. */
   struct sched_node *def; /** Defining node in the region, if any. */
   unsigned uses_total; /** Uses in the whole function. */
   unsigned uses_region; /** Uses in the region. */
   unsigned uses_left; /** Uses in the region not scheduled yet. */
};

struct sched_vreg {
   unsigned region;
   struct sched_node *last_write;
   struct util_dynarray reads; /** struct sched_node *, since last_write */
};

struct sched_ctx {
   void *mem_ctx;
   struct sched_ssa *ssa;
   struct sched_vreg *vreg;
   unsigned region;
   struct sched_node **sim; /** Scratch list for sched_fits(). */
   int *cap; /** Pressure allowed at each original position. */
   bool pinned_moved; /** Whether a pinned run was pulled up. */
   unsigned cap_window; /** See SCHED_CAP_WINDOW. */
};

/**
 * \brief Returns whether an instruction only reads its sources and writes its
 *        SSA/vreg destinations, so that it can be reordered with its peers.
 */
static bool sched_is_alu(pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_FADD:
   case PCO_OP_FMUL:
   case PCO_OP_FMAD:
   case PCO_OP_FRCP:
   case PCO_OP_FRSQ:
   case PCO_OP_FLOG:
   case PCO_OP_FLOGCN:
   case PCO_OP_FEXP:
   case PCO_OP_MBYP:
   case PCO_OP_MOVS1:
   case PCO_OP_PCK:
   case PCO_OP_UNPCK:
   case PCO_OP_ADD64_32:
   case PCO_OP_IMADD64:
   case PCO_OP_IMADD32:
   case PCO_OP_MOVI32:
   case PCO_OP_CBS:
   case PCO_OP_FTB:
   case PCO_OP_REV:
   case PCO_OP_LOGICAL:
   case PCO_OP_SHIFT:
   case PCO_OP_COPYSIGN:
   case PCO_OP_IBFE:
   case PCO_OP_UBFE:
   case PCO_OP_BFI:
   case PCO_OP_SCMP:
   case PCO_OP_BCMP:
   case PCO_OP_BCSEL:
   case PCO_OP_CSEL:
   case PCO_OP_PSEL:
   case PCO_OP_PSEL_TRIG:
   case PCO_OP_FSIGN:
   case PCO_OP_ISIGN:
   case PCO_OP_FCEIL:
   case PCO_OP_MIN:
   case PCO_OP_MAX:
   case PCO_OP_IADD32:
   case PCO_OP_IMUL32:
   case PCO_OP_UADD_CARRY:
   case PCO_OP_UADD_SAT:
   case PCO_OP_FNEG:
   case PCO_OP_FABS:
   case PCO_OP_FFLR:
   case PCO_OP_MOV:
   case PCO_OP_VEC:
   case PCO_OP_COMP:
      break;

   default:
      return false;
   }

   if (pco_instr_has_rpt(instr) && pco_instr_get_rpt(instr) > 1)
      return false;

   if (pco_instr_has_olchk(instr) && pco_instr_get_olchk(instr))
      return false;

   pco_foreach_instr_dest (pdest, instr) {
      if (!pco_ref_is_null(*pdest) && !pco_ref_is_ssa(*pdest) &&
          !pco_ref_is_vreg(*pdest))
         return false;
   }

   /* Hardware registers are only written by instructions that end a region,
    * so reading them inside a region does not depend on the order.
    */
   pco_foreach_instr_src (psrc, instr) {
      if (!pco_ref_is_null(*psrc) && !pco_ref_is_ssa(*psrc) &&
          !pco_ref_is_reg(*psrc) && !pco_ref_is_imm(*psrc))
         return false;
   }

   return true;
}

/**
 * \brief Returns whether a non-ALU instruction can sit inside a region.
 *
 * Such instructions (memory, sample and iteration ops and their data fence
 * waits) keep their order among themselves; ALU code only moves across them
 * following its data dependencies. They write SSA values only and do not
 * touch predicates.
 */
static bool sched_is_pinnable(pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_SMP:
   case PCO_OP_LD:
   case PCO_OP_LD_REGBL:
   case PCO_OP_ST:
   case PCO_OP_ST_REGBL:
   case PCO_OP_ST32:
   case PCO_OP_ST32_REGBL:
   case PCO_OP_WDF:
   case PCO_OP_IDF:
   case PCO_OP_FITR:
   case PCO_OP_FITRP:
   case PCO_OP_DITR:
   case PCO_OP_DITRP:
   case PCO_OP_FDSX:
   case PCO_OP_FDSXF:
   case PCO_OP_FDSY:
   case PCO_OP_FDSYF:
      break;

   default:
      return false;
   }

   pco_foreach_instr_dest (pdest, instr) {
      if (!pco_ref_is_null(*pdest) && !pco_ref_is_ssa(*pdest) &&
          !pco_ref_is_vreg(*pdest))
         return false;
   }

   pco_foreach_instr_src (psrc, instr) {
      if (!pco_ref_is_null(*psrc) && !pco_ref_is_ssa(*psrc) &&
          !pco_ref_is_reg(*psrc) && !pco_ref_is_imm(*psrc) &&
          !pco_ref_is_drc(*psrc))
         return false;
   }

   return true;
}

static int sched_drc(const pco_instr *instr)
{
   pco_foreach_instr_src (psrc, instr) {
      if (pco_ref_is_drc(*psrc))
         return pco_ref_get_drc(*psrc);
   }

   return -1;
}

/**
 * \brief Returns whether an op usually becomes a lone phase 0 op which another
 *        op can later be co-issued with.
 */
static bool sched_is_pairable(const pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_FADD:
   case PCO_OP_FMUL:
   case PCO_OP_FMAD:
   case PCO_OP_MBYP:
   case PCO_OP_MOV:
   case PCO_OP_FNEG:
   case PCO_OP_FABS:
      return true;

   default:
      break;
   }

   return false;
}

static void sched_add_edge(struct sched_node *from, struct sched_node *to)
{
   if (!from || from == to)
      return;

   util_dynarray_foreach (&from->succs, struct sched_node *, succ) {
      if (*succ == to)
         return;
   }

   util_dynarray_append_typed(&from->succs, struct sched_node *, to);
   util_dynarray_append_typed(&to->preds, struct sched_node *, from);
   ++to->num_preds;
}

static bool sched_is_succ(const struct sched_node *pred,
                          const struct sched_node *node)
{
   util_dynarray_foreach (&pred->succs, struct sched_node *, succ) {
      if (*succ == node)
         return true;
   }

   return false;
}

/** Register units an SSA value occupies. */
static int sched_ssa_units(pco_ref ref)
{
   return MAX2(pco_ref_get_chans(ref), 1) *
          DIV_ROUND_UP(MAX2(pco_ref_get_bits(ref), 32), 32);
}

static struct sched_ssa *sched_get_ssa(struct sched_ctx *ctx, unsigned index)
{
   struct sched_ssa *ssa = &ctx->ssa[index];
   if (ssa->region != ctx->region) {
      ssa->region = ctx->region;
      ssa->def = NULL;
      ssa->uses_region = 0;
      ssa->uses_left = 0;
   }

   return ssa;
}

static struct sched_vreg *sched_get_vreg(struct sched_ctx *ctx, unsigned index)
{
   struct sched_vreg *vreg = &ctx->vreg[index];
   if (vreg->region != ctx->region) {
      vreg->region = ctx->region;
      vreg->last_write = NULL;
      util_dynarray_clear(&vreg->reads);
   }

   return vreg;
}

/** Returns whether src is the first source of the node reading its value. */
static bool sched_first_use(const pco_instr *instr, const pco_ref *src)
{
   for (const pco_ref *p = instr->src; p < src; ++p) {
      if (pco_ref_is_ssa(*p) && p->val == src->val)
         return false;
   }

   return true;
}

/**
 * \brief Returns how many register units die when a node is scheduled.
 */
static int sched_kills(struct sched_ctx *ctx, const struct sched_node *node)
{
   int kills = 0;

   pco_foreach_instr_src_ssa (psrc, node->instr) {
      if (!sched_first_use(node->instr, psrc))
         continue;

      unsigned count = 0;
      pco_foreach_instr_src_ssa (other, node->instr) {
         if (other->val == psrc->val)
            ++count;
      }

      struct sched_ssa *ssa = &ctx->ssa[psrc->val];
      if (ssa->uses_total == ssa->uses_region && ssa->uses_left == count)
         kills += sched_ssa_units(*psrc);
   }

   return kills;
}

static void sched_retire(struct sched_ctx *ctx, struct sched_node *node)
{
   pco_foreach_instr_src_ssa (psrc, node->instr) {
      --ctx->ssa[psrc->val].uses_left;
   }
}

/**
 * \brief Returns whether node copies the result of pred, which the
 *        co-issue pass can fold into pred's igrp.
 */
static bool sched_is_copy_of(const struct sched_node *node,
                             const struct sched_node *pred)
{
   const pco_instr *instr = node->instr;
   if (instr->op != PCO_OP_MBYP && instr->op != PCO_OP_MOV)
      return false;

   return pco_ref_is_ssa(instr->src[0]) &&
          pco_ref_is_ssa(pred->instr->dest[0]) &&
          instr->src[0].val == pred->instr->dest[0].val;
}

/**
 * \brief Returns whether the run of originally consecutive pinned nodes
 *        starting at start only waits on nodes of the run itself.
 */
static bool sched_run_ready(struct sched_node *nodes,
                            unsigned num_nodes,
                            unsigned start)
{
   for (unsigned k = start; k < num_nodes && nodes[k].pinned; ++k) {
      unsigned internal = 0;
      for (unsigned j = start; j < k; ++j) {
         if (sched_is_succ(&nodes[j], &nodes[k]))
            ++internal;
      }

      if (nodes[k].num_preds != internal)
         return false;
   }

   return true;
}

/**
 * \brief Returns the ready, unpinned node with the lowest original position
 *        among the unscheduled ancestors of the pinned run starting at start.
 */
static struct sched_node *sched_run_ancestor(struct sched_node *nodes,
                                             unsigned num_nodes,
                                             unsigned start,
                                             unsigned visit)
{
   struct sched_node *best = NULL;
   struct util_dynarray stack;
   util_dynarray_init(&stack, NULL);

   for (unsigned k = start; k < num_nodes && nodes[k].pinned; ++k)
      util_dynarray_append_typed(&stack, struct sched_node *, &nodes[k]);

   while (util_dynarray_num_elements(&stack, struct sched_node *)) {
      struct sched_node *node =
         util_dynarray_pop(&stack, struct sched_node *);

      util_dynarray_foreach (&node->preds, struct sched_node *, ppred) {
         struct sched_node *pred = *ppred;
         if (pred->scheduled || pred->visit == visit)
            continue;

         pred->visit = visit;
         if (!pred->num_preds && !pred->pinned &&
             (!best || pred->orig < best->orig))
            best = pred;

         util_dynarray_append_typed(&stack, struct sched_node *, pred);
      }
   }

   util_dynarray_fini(&stack);
   return best;
}

/**
 * \brief Returns whether scheduling cand now, then the rest of the region in
 *        its original order, keeps the live register units within cap.
 *
 * Only accepting such moves keeps the original order a valid fallback at every
 * step, so the peak pressure of the region never grows.
 */
static bool sched_fits(struct sched_ctx *ctx,
                       struct sched_node *nodes,
                       unsigned num_nodes,
                       unsigned next,
                       struct sched_node *cand,
                       int live)
{
   /* A pinned candidate brings the rest of its pinned run along. */
   unsigned cand_end = cand->orig + 1;
   if (cand->pinned) {
      while (cand_end < num_nodes && nodes[cand_end].pinned)
         ++cand_end;
   }

   unsigned num_sim = 0;
   bool fits = true;

   for (unsigned pass = 0; pass < 2 && fits; ++pass) {
      unsigned from = pass ? next : cand->orig;
      unsigned to = pass ? num_nodes : cand_end;

      for (unsigned n = from; n < to; ++n) {
         struct sched_node *node = &nodes[n];
         bool in_cand = n >= cand->orig && n < cand_end;
         if (node->scheduled || (pass && in_cand))
            continue;

         /* Pulled nodes run at the current position. */
         int cap = ctx->cap[pass ? n : next];
         if (live + node->defs > cap) {
            fits = false;
            break;
         }

         live += node->defs - sched_kills(ctx, node);
         sched_retire(ctx, node);
         ctx->sim[num_sim++] = node;
      }
   }

   /* Undo the simulated uses. */
   for (unsigned n = 0; n < num_sim; ++n) {
      pco_foreach_instr_src_ssa (psrc, ctx->sim[n]->instr) {
         ++ctx->ssa[psrc->val].uses_left;
      }
   }

   return fits;
}

/**
 * \brief List-schedules one region of ALU instructions.
 *
 * Keeps the original order, except that after a pairable op the next
 * independent pairable op of the window is pulled up, so that the post-RA
 * co-issue pass finds phase 0/phase 1 pairs, and immediate loads are pulled
 * up so that they do not separate two candidates. A move is only taken if the
 * peak number of live SSA register units stays within the one of the original
 * order.
 *
 * \return True if the order changed.
 */
static bool sched_region(struct sched_ctx *ctx,
                         pco_instr **instrs,
                         unsigned num_instrs,
                         unsigned drcs_in_flight)
{
   ++ctx->region;

   struct sched_node *nodes = rzalloc_array(ctx->mem_ctx,
                                            struct sched_node,
                                            num_instrs);
   ctx->sim = ralloc_array(ctx->mem_ctx, struct sched_node *, num_instrs);

   struct sched_node *last_pinned = NULL;
   unsigned start_drcs = drcs_in_flight;

   for (unsigned n = 0; n < num_instrs; ++n) {
      struct sched_node *node = &nodes[n];
      pco_instr *instr = instrs[n];

      node->instr = instr;
      node->orig = n;
      util_dynarray_init(&node->succs, ctx->mem_ctx);
      util_dynarray_init(&node->preds, ctx->mem_ctx);

      /* ALU code between a data fence op and its wait stays there, and no
       * other ALU code moves there: the op may still read its sources or
       * write its destinations.
       */
      node->pinned = !sched_is_alu(instr) || drcs_in_flight;
      if (node->pinned) {
         sched_add_edge(last_pinned, node);
         last_pinned = node;
      }

      int drc = sched_drc(instr);
      if (drc >= 0) {
         if (instr->op == PCO_OP_WDF)
            drcs_in_flight &= ~BITFIELD_BIT(drc);
         else
            drcs_in_flight |= BITFIELD_BIT(drc);
      }

      pco_foreach_instr_src_ssa (psrc, instr) {
         struct sched_ssa *ssa = sched_get_ssa(ctx, psrc->val);
         ++ssa->uses_region;
         ++ssa->uses_left;
         sched_add_edge(ssa->def, node);
      }

      pco_foreach_instr_src_vreg (psrc, instr) {
         struct sched_vreg *vreg = sched_get_vreg(ctx, psrc->val);
         sched_add_edge(vreg->last_write, node);
         util_dynarray_append_typed(&vreg->reads, struct sched_node *, node);
      }

      pco_foreach_instr_dest_vreg (pdest, instr) {
         struct sched_vreg *vreg = sched_get_vreg(ctx, pdest->val);
         sched_add_edge(vreg->last_write, node);
         util_dynarray_foreach (&vreg->reads, struct sched_node *, read) {
            sched_add_edge(*read, node);
         }
         util_dynarray_clear(&vreg->reads);
         vreg->last_write = node;
      }

      pco_foreach_instr_dest_ssa (pdest, instr) {
         struct sched_ssa *ssa = sched_get_ssa(ctx, pdest->val);
         ssa->def = node;
         if (ssa->uses_total)
            node->defs += sched_ssa_units(*pdest);
      }
   }

   /* Pressure of the original order, relative to the region start. Each
    * position may reach the highest original pressure of its neighbourhood.
    */
   int live = 0;
   int *peak = ralloc_array(ctx->mem_ctx, int, num_instrs);
   for (unsigned n = 0; n < num_instrs; ++n) {
      peak[n] = live + nodes[n].defs;
      live += nodes[n].defs - sched_kills(ctx, &nodes[n]);
      sched_retire(ctx, &nodes[n]);
   }
   ctx->cap = ralloc_array(ctx->mem_ctx, int, num_instrs);
   int region_peak = 0;
   for (unsigned n = 0; n < num_instrs; ++n)
      region_peak = MAX2(region_peak, peak[n]);

   for (unsigned n = 0; n < num_instrs; ++n) {
      unsigned w = ctx->cap_window;
      if (w >= num_instrs) {
         ctx->cap[n] = region_peak;
         continue;
      }

      unsigned from = n > w ? n - w : 0;
      unsigned to = MIN2(n + w + 1, num_instrs);
      ctx->cap[n] = 0;
      for (unsigned k = from; k < to; ++k)
         ctx->cap[n] = MAX2(ctx->cap[n], peak[k]);
   }

   for (unsigned n = 0; n < num_instrs; ++n) {
      pco_foreach_instr_src_ssa (psrc, instrs[n]) {
         ++ctx->ssa[psrc->val].uses_left;
      }
   }

   struct sched_node **order =
      ralloc_array(ctx->mem_ctx, struct sched_node *, num_instrs);
   struct sched_node *last = NULL;
   bool open = false;
   bool changed = false;
   live = 0;

   unsigned next = 0;
   unsigned next_pinned = 0;
   drcs_in_flight = start_drcs;
   for (unsigned s = 0; s < num_instrs; ++s) {
      while (nodes[next].scheduled)
         ++next;

      unsigned end = MIN2(next + SCHED_WINDOW, num_instrs);
      struct sched_node *best = NULL;
      bool best_pair = false;

      /* Pinned instructions that were next to each other stay together, so
       * a data fence op is always directly followed by its wait.
       */
      if (last && last->pinned && last->orig + 1 < num_instrs &&
          nodes[last->orig + 1].pinned) {
         best = &nodes[last->orig + 1];
         assert(!best->num_preds);
      }

      /* After a pairable op, pull up an independent pairable op (or a copy of
       * its result) for the co-issue pass.
       */
      for (unsigned n = next + 1; open && !best && !drcs_in_flight && n < end;
           ++n) {
         struct sched_node *node = &nodes[n];
         if (node->scheduled || node->num_preds || node->pinned)
            continue;

         if (sched_is_copy_of(node, last) ||
             (sched_is_pairable(node->instr) && !sched_is_succ(last, node))) {
            if (sched_fits(ctx, nodes, num_instrs, next, node, live)) {
               best = node;
               best_pair = true;
            }
            break;
         }
      }

      /* Issue the next pinned instruction as soon as its sources are ready,
       * so that the ALU code using its result can be interleaved with the
       * code before it.
       */
      while (next_pinned < num_instrs &&
             (!nodes[next_pinned].pinned || nodes[next_pinned].scheduled))
         ++next_pinned;

      if (!best && next_pinned < num_instrs && next_pinned != next &&
          next_pinned < next + SCHED_PIN_WINDOW) {
         struct sched_node *cand;
         if (sched_run_ready(nodes, num_instrs, next_pinned))
            cand = &nodes[next_pinned];
         else if (!drcs_in_flight)
            cand = sched_run_ancestor(nodes, num_instrs, next_pinned, s + 1);
         else
            cand = NULL;

         if (cand && cand != &nodes[next] &&
             sched_fits(ctx, nodes, num_instrs, next, cand, live)) {
            best = cand;
            ctx->pinned_moved = true;
         }
      }

      /* Load immediates early so that their users do not depend on a group
       * sitting between two co-issue candidates.
       */
      for (unsigned n = next + 1; !best && !drcs_in_flight && n < end; ++n) {
         struct sched_node *node = &nodes[n];
         if (node->scheduled || node->num_preds || node->pinned ||
             node->instr->op != PCO_OP_MOVI32)
            continue;

         if (sched_fits(ctx, nodes, num_instrs, next, node, live))
            best = node;
         break;
      }

      /* Otherwise keep the original order, which always fits. */
      if (!best) {
         best = &nodes[next];
         best_pair = open && (sched_is_copy_of(best, last) ||
                              (sched_is_pairable(best->instr) &&
                               !sched_is_succ(last, best)));
      }

      assert(best && !best->num_preds);
      assert(best->pinned || !drcs_in_flight);

      int drc = sched_drc(best->instr);
      if (drc >= 0) {
         if (best->instr->op == PCO_OP_WDF)
            drcs_in_flight &= ~BITFIELD_BIT(drc);
         else
            drcs_in_flight |= BITFIELD_BIT(drc);
      }

      live += best->defs - sched_kills(ctx, best);
      sched_retire(ctx, best);
      best->scheduled = true;
      util_dynarray_foreach (&best->succs, struct sched_node *, succ) {
         --(*succ)->num_preds;
      }

      changed |= best->orig != s;
      order[s] = best;

      /* A pairable op opens a slot, its partner closes it. */
      open = !best_pair && sched_is_pairable(best->instr);
      last = best;
   }

   if (changed) {
      struct list_head *anchor = instrs[0]->link.prev;
      for (unsigned s = 0; s < num_instrs; ++s) {
         pco_instr *instr = order[s]->instr;
         list_del(&instr->link);
         list_add(&instr->link, anchor);
         anchor = &instr->link;
      }
   }

   return changed;
}

/**
 * \brief Reorders straight-line ALU code before register allocation to
 *        interleave independent dependency chains.
 *
 * \param[in,out] shader PCO shader.
 * \return True if the pass made progress.
 */
#define PCO_SCHED_MAX_INSTRS 4096U

bool pco_schedule_alu(pco_shader *shader)
{
   bool progress = false;
   bool pinned = !PCO_DEBUG(NO_SCHED_PINNED) && !shader->sched_no_pinned;

   if (PCO_DEBUG(NO_SCHED) || shader->no_sched)
      return false;

   /* Skip very large shaders: the gain is small there, while scheduling
    * means compiling them twice (see pco_compile_nir()).
    */
   unsigned num_instrs = 0;
   pco_foreach_func_in_shader (func, shader) {
      pco_foreach_instr_in_func (instr, func) {
         ++num_instrs;
      }
   }

   if (num_instrs > PCO_SCHED_MAX_INSTRS)
      return false;

   pco_foreach_func_in_shader (func, shader) {
      void *mem_ctx = ralloc_context(NULL);
      struct sched_ctx ctx = {
         .mem_ctx = mem_ctx,
         .ssa = rzalloc_array(mem_ctx, struct sched_ssa, func->next_ssa),
         .vreg = rzalloc_array(mem_ctx, struct sched_vreg, func->next_vreg),
         .cap_window = pinned ? SCHED_CAP_WINDOW : UINT_MAX,
      };

      for (unsigned u = 0; u < func->next_vreg; ++u)
         util_dynarray_init(&ctx.vreg[u].reads, mem_ctx);

      pco_foreach_instr_in_func (instr, func) {
         pco_foreach_instr_src_ssa (psrc, instr) {
            ++ctx.ssa[psrc->val].uses_total;
         }
      }

      struct util_dynarray region;
      util_dynarray_init(&region, mem_ctx);

      pco_foreach_block_in_func (block, func) {
         enum pco_exec_cnd region_cnd = PCO_EXEC_CND_E1_ZX;
         unsigned block_drcs = 0, region_drcs = 0;
         pco_instr *next;
         for (pco_instr *instr =
                 list_first_entry(&block->instrs, pco_instr, link);
              &instr->link != &block->instrs;
              instr = next) {
            next = list_entry(instr->link.next, pco_instr, link);

            /* A region holds instructions under the same predicate; nothing
             * in it writes p0.
             */
            bool alu = sched_is_alu(instr) ||
                       (pinned && sched_is_pinnable(instr));
            enum pco_exec_cnd cnd = pco_instr_has_exec_cnd(instr)
                                       ? pco_instr_get_exec_cnd(instr)
                                       : PCO_EXEC_CND_E1_ZX;
            unsigned num = util_dynarray_num_elements(&region, pco_instr *);

            if (num && (!alu || cnd != region_cnd)) {
               if (num > 2) {
                  progress |= sched_region(&ctx,
                                           util_dynarray_begin(&region),
                                           num,
                                           region_drcs);
               }
               util_dynarray_clear(&region);
               num = 0;
            }

            if (alu) {
               if (!num)
                  region_drcs = block_drcs;

               region_cnd = cnd;
               util_dynarray_append_typed(&region, pco_instr *, instr);
            }

            /* Data fence ops still in flight where the next region starts. */
            int drc = sched_drc(instr);
            if (drc >= 0) {
               if (instr->op == PCO_OP_WDF)
                  block_drcs &= ~BITFIELD_BIT(drc);
               else
                  block_drcs |= BITFIELD_BIT(drc);
            }
         }

         unsigned num = util_dynarray_num_elements(&region, pco_instr *);
         if (num > 2) {
            progress |= sched_region(&ctx,
                                     util_dynarray_begin(&region),
                                     num,
                                     region_drcs);
         }
         util_dynarray_clear(&region);
      }

      shader->sched_pinned_moved |= ctx.pinned_moved;
      ralloc_free(mem_ctx);
   }

   shader->sched_changed |= progress;
   return progress;
}
