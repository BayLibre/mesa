/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file pco_ir.c
 *
 * \brief PCO IR-specific functions.
 */

#include "pco.h"
#include "pco_internal.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * \brief Runs passes on a PCO shader.
 *
 * \param[in] ctx PCO compiler context.
 * \param[in,out] shader PCO shader.
 */
void pco_process_ir(pco_ctx *ctx, pco_shader *shader)
{
   pco_validate_shader(shader, "before passes");

   PCO_PASS(_, shader, pco_const_imms);
   PCO_PASS(_, shader, pco_opt);

   bool progress;
   do {
      progress = false;
      PCO_PASS(progress, shader, pco_dce);
   } while (progress);

   PCO_PASS(_, shader, pco_bool);
   PCO_PASS(_, shader, pco_cf);

   PCO_PASS(_, shader, pco_shrink_vecs);

   PCO_PASS(_, shader, pco_const_imms);
   PCO_PASS(_, shader, pco_opt_comp_only_vecs);
   PCO_PASS(_, shader, pco_opt);

   do {
      progress = false;
      PCO_PASS(progress, shader, pco_dce);
   } while (progress);

   /* TODO: schedule after RA instead as e.g. vecs may no longer be the first
    * time a drc result is used.
    */
   PCO_PASS(_, shader, pco_shared_imms);
   PCO_PASS(_, shader, pco_schedule);
   PCO_PASS(_, shader, pco_pre_ra_legalize);
   PCO_PASS(_, shader, pco_reuse_imms);
   if (pco_cse_vecs(shader)) {
      do {
         progress = false;
         PCO_PASS(progress, shader, pco_dce);
      } while (progress);
   }
   PCO_PASS(_, shader, pco_schedule_alu);
   PCO_PASS(_, shader, pco_ra);
   PCO_PASS(_, shader, pco_post_ra_legalize);
   PCO_PASS(_, shader, pco_end);
   PCO_PASS(_, shader, pco_group_instrs);

   pco_validate_shader(shader, "after passes");

   if (pco_should_print_shader(shader))
      pco_print_shader(shader, stdout, "after passes");
}

static unsigned count_igrps(pco_shader *shader)
{
   unsigned count = 0;

   pco_foreach_func_in_shader (func, shader) {
      pco_foreach_igrp_in_func (igrp, func) {
         ++count;
      }
   }

   return count;
}

/**
 * \brief Returns whether shader a is a better result than shader b: fewer
 *        spills, then fewer temp allocation blocks (which bound occupancy),
 *        then fewer instruction groups.
 */
static bool shader_is_better(pco_shader *a, pco_shader *b)
{
   const pco_common_data *ca = &a->data.common;
   const pco_common_data *cb = &b->data.common;

   if (ca->spilled_temps != cb->spilled_temps)
      return ca->spilled_temps < cb->spilled_temps;

   unsigned blocks_a = DIV_ROUND_UP(ca->temps, 4);
   unsigned blocks_b = DIV_ROUND_UP(cb->temps, 4);
   if (blocks_a != blocks_b)
      return blocks_a < blocks_b;

   return count_igrps(a) < count_igrps(b);
}

/**
 * \brief Translates, processes and encodes a NIR shader.
 *
 * When the pre-RA scheduler changed the instruction order, the shader is
 * compiled a second time without it, and the scheduled result is only kept
 * if it does not spill more, does not need more temp allocation blocks and
 * does not have more instruction groups. When the unscheduled one wins, its
 * final IR is printed after the scheduled one.
 *
 * \param[in] ctx PCO compiler context.
 * \param[in] nir NIR shader.
 * \param[in] data Shader data.
 * \param[in] mem_ctx Ralloc memory context.
 * \return The PCO shader, or NULL on failure.
 */
pco_shader *
pco_compile_nir(pco_ctx *ctx, nir_shader *nir, pco_data *data, void *mem_ctx)
{
   pco_shader *shader = pco_trans_nir(ctx, nir, data, mem_ctx);
   if (!shader)
      return NULL;

   pco_process_ir(ctx, shader);

   if (shader->sched_changed && !PCO_DEBUG(NO_SCHED_CHECK)) {
      pco_shader *unsched = pco_trans_nir(ctx, nir, data, mem_ctx);
      if (unsched) {
         unsched->quiet = true;
         unsched->no_sched = true;
         pco_process_ir(ctx, unsched);

         if (shader_is_better(unsched, shader)) {
            ralloc_free(shader);
            shader = unsched;

            shader->quiet = false;
            if (pco_should_print_shader(shader)) {
               printf("pco: kept the unscheduled shader\n");
               pco_print_shader(shader, stdout, "after passes");
            }
         } else {
            ralloc_free(unsched);
         }
      }
   }

   pco_encode_ir(ctx, shader);
   return shader;
}
