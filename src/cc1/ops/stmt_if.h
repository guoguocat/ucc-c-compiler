STMT_DEFS(if);


void flow_fold(
		stmt_flow *flow,
		symtable **pstab,
		stmt_fold_ctx_block *ctx);

basic_blk *flow_gen(
		stmt_flow *flow, symtable *stab,
		basic_blk *bb) ucc_wur;
