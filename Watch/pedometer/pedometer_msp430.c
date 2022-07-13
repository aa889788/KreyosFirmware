#include "msp430.h"

/**
* @brief <b>Description: Q15 multiplication</b>
* @param[in] a
* @param[in] *var
**/
void do_q15_mult(signed int a, signed int * var)
{
//  WDTCTL = WDTPW+WDTHOLD;                   // Stop WDT

  MPY32CTL0 = MPYFRAC;                      // Set fractional mode
  MPYS = a;                            // Load first operand
  OP2 = *(var);                             // Load second operand
  *(var) = RESHI;                       // Q15 result
  
  MPY32CTL0 &= ~MPYFRAC;
}
