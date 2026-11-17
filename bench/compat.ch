/* For compilers other than xbc (the comparison in bench/compare.sh): what the benchmarks use of xbase's own, in
 * standard terms. ElapsedMs(): milliseconds from the date and Seconds(), so that a run past midnight counts up. */
#ifndef __XBASE__
#xtranslate ElapsedMs() => ( ( Date() - SToD( "20000101" ) ) * 86400000 + Int( Seconds() * 1000 ) )
#endif
