//
// rsp/pipeline.c: RSP processor pipeline.
//
// CEN64: Cycle-Accurate Nintendo 64 Simulator.
// Copyright (C) 2014, Tyler J. Stachecki.
//
// This file is subject to the terms and conditions defined in
// 'LICENSE', which is part of this source code package.
//

#include "common.h"
#include "rsp/cp0.h"
#include "rsp/cpu.h"

// Prints out instructions and their address as they are executed.
//#define PRINT_EXEC

typedef void (*pipeline_function)(struct rsp *rsp);

// Instruction cache fetch stage.
static inline void rsp_if_stage(struct rsp *rsp) {
  struct rsp_ifrd_latch *ifrd_latch = &rsp->pipeline.ifrd_latch;
  uint32_t pc = ifrd_latch->pc;
  uint32_t iw;

  memcpy(&iw, rsp->mem + 0x1000 + pc, sizeof(iw));
  iw = byteswap_32(iw);

  ifrd_latch->common.pc = pc;
  ifrd_latch->pc = (pc + 4) & 0xFFC;
  ifrd_latch->iw = iw;
}

// Register fetch and decode stage.
static inline void rsp_rd_stage(struct rsp *rsp) {
  struct rsp_rdex_latch *rdex_latch = &rsp->pipeline.rdex_latch;
  struct rsp_ifrd_latch *ifrd_latch = &rsp->pipeline.ifrd_latch;

  uint32_t iw = ifrd_latch->iw;

  rdex_latch->common = ifrd_latch->common;
  rdex_latch->opcode = *rsp_decode_instruction(iw);
  rdex_latch->iw = ifrd_latch->iw;
}

// Execution stage.
static inline void rsp_ex_stage(struct rsp *rsp) {
  struct rsp_dfwb_latch *dfwb_latch = &rsp->pipeline.dfwb_latch;
  struct rsp_exdf_latch *exdf_latch = &rsp->pipeline.exdf_latch;
  struct rsp_rdex_latch *rdex_latch = &rsp->pipeline.rdex_latch;

  uint64_t rs_reg, rt_reg, temp;
  unsigned rs, rt;
  uint32_t iw;

  exdf_latch->common = rdex_latch->common;
  iw = rdex_latch->iw;

  rs = GET_RS(iw);
  rt = GET_RT(iw);

  // Forward results from DF/WB.
  temp = rsp->regs[dfwb_latch->dest];
  rsp->regs[dfwb_latch->dest] = dfwb_latch->result;
  rsp->regs[RSP_REGISTER_R0] = 0x00000000U;

  rs_reg = rsp->regs[rs];
  rt_reg = rsp->regs[rt];

  rsp->regs[dfwb_latch->dest] = temp;

  // Finally, execute the instruction.
#ifdef PRINT_EXEC
  debug("%.8X: %s\n", rdex_latch->common.pc,
    rsp_opcode_mnemonics[rdex_latch->opcode.id]);
#endif

  exdf_latch->dest = RSP_REGISTER_R0;
  return rsp_function_table[rdex_latch->opcode.id](
    rsp, iw, rs_reg, rt_reg);
}

// Data cache fetch stage.
static inline void rsp_df_stage(struct rsp *rsp) {
  struct rsp_dfwb_latch *dfwb_latch = &rsp->pipeline.dfwb_latch;
  struct rsp_exdf_latch *exdf_latch = &rsp->pipeline.exdf_latch;

  dfwb_latch->common = exdf_latch->common;

  dfwb_latch->result = exdf_latch->result;
  dfwb_latch->dest = exdf_latch->dest;
}

// Writeback stage.
static inline void rsp_wb_stage(struct rsp *rsp) {
  const struct rsp_dfwb_latch *dfwb_latch = &rsp->pipeline.dfwb_latch;

  rsp->regs[dfwb_latch->dest] = dfwb_latch->result;
  rsp->regs[RSP_REGISTER_R0] = 0x00000000U;
}

// Advances the processor pipeline by one clock.
void rsp_cycle(struct rsp *rsp) {
  struct rsp_pipeline *pipeline = &rsp->pipeline;

  if (rsp->regs[RSP_CP0_REGISTER_SP_STATUS] & SP_STATUS_HALT)
    return;

  rsp_wb_stage(rsp);
  rsp_df_stage(rsp);
  rsp_ex_stage(rsp);
  rsp_rd_stage(rsp);
  rsp_if_stage(rsp);
}

// Initializes the pipeline with default values.
void rsp_pipeline_init(struct rsp_pipeline *pipeline) {

}

