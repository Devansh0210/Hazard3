# See LICENSE for license details.

#*****************************************************************************
# add.S
#-----------------------------------------------------------------------------
#
# Test add instruction.
#

#include "riscv_test.h"
#include "test_macros.h"

RVTEST_RV64U
RVTEST_CODE_BEGIN

li gp, 2
li ra, 0
li sp, 0
li a1, 0
P0:
    beq ra,sp,P1
    nop
    nop
P3: 
    nop
    nop
    bne ra,sp,fail
    nop
    add t0,t1,t2
    nop
P1:
    beq ra, sp, P3
    

  # TEST_BR2_OP_TAKEN( 2, beq,  0,  0 );
  # TEST_BR2_OP_TAKEN( 3, beq,  1,  1 );
  # TEST_BR2_OP_NOTTAKEN( 6, beq,  1,  0 );
  # TEST_BR2_OP_NOTTAKEN( 7, beq, -1,  1 );
  #
  TEST_PASSFAIL

# TEST_PASSFAIL
RVTEST_CODE_END

  .data
RVTEST_DATA_BEGIN
# .data 010

  TEST_DATA

RVTEST_DATA_END
