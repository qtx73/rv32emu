int decode_rv32i_instr(uint32_t);
int decode_rv32m_instr(uint32_t);
int decode_rvv_instr(uint32_t);

#define DEBUG
#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif