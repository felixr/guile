#include "test.h"

static void
run_test(jit_state_t *j, uint8_t *arena_base, size_t arena_size)
{
#if __WORDSIZE > 32
  jit_begin(j, arena_base, arena_size);

  const jit_arg_abi_t abi[] = { JIT_ARG_ABI_INTMAX };
  jit_arg_t args[1];
  const jit_anyreg_t regs[] = { { .gpr=JIT_R1 } };

  jit_receive(j, 1, abi, args);
  jit_load_args(j, 1, abi, args, regs);

  jit_extr_ui(j, JIT_R0, JIT_R1);
  jit_retr(j, JIT_R0);

  uintmax_t (*f)(uintmax_t) = jit_end(j, NULL);

  ASSERT(f(0) == 0);
  ASSERT(f(1) == 1);
  ASSERT(f(0xffffffff) == 0xffffffff);
  ASSERT(f(0xfffffffff) == 0xffffffff);
  ASSERT(f(0xf00000000) == 0);
#endif
}

int
main (int argc, char *argv[])
{
  return main_helper(argc, argv, run_test);
}
