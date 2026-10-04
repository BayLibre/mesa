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
