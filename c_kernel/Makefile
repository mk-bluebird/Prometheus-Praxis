# c_kernel/Makefile
CFLAGS = -std=c17 -O2 -Wall -Wextra \
         -fno-fast-math \
         -ffp-contract=off \
         -frounding-math \
         -fsignaling-nans

# Verify floating-point consistency
test-fp-determinism:
	$(CC) $(CFLAGS) -DTEST_FP fp_test.c -o fp_test
	./fp_test --verify-ieee754
