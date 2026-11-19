// Massive random-number generation: 30,000,000 rounds of four generators in integer arithmetic (a 31-bit linear
// congruential generator, a 64-bit xorshift with masks for the logical right shift, Marsaglia's two 16-bit
// multiply-with-carry streams and a 32-bit xorshift), their sums, a histogram of 256 buckets of their top bytes and
// the sum of the congruential numbers scaled to [0, 1); the sums, the histogram's checksum and the scaled sum are
// checked.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), n := 30000000, hist := AFill( Array( 256 ), 0 ), i, r, ok
   LOCAL lcg := 12345, xs64 := 88172645463325252, z := 362436069, w := 521288629, xs32 := 2463534242
   LOCAL s1 := 0, s2 := 0, s3 := 0, s4 := 0, fsum := 0.0, check := 0
   FOR i := 1 TO n
      lcg := ( lcg * 1103515245 + 12345 ) & 0x7FFFFFFF
      s1 += lcg
      hist[ ( lcg >> 23 ) + 1 ]++
      fsum += lcg / 2147483648.0

      xs64 := xs64 ^^ ( xs64 << 13 )
      xs64 := xs64 ^^ ( ( xs64 >> 7 ) & 0x01FFFFFFFFFFFFFF )
      xs64 := xs64 ^^ ( xs64 << 17 )
      r := ( xs64 >> 32 ) & 0xFFFFFFFF
      s2 += r
      hist[ ( r >> 24 ) + 1 ]++

      z := 36969 * ( z & 0xFFFF ) + ( z >> 16 )
      w := 18000 * ( w & 0xFFFF ) + ( w >> 16 )
      r := ( ( z << 16 ) + w ) & 0xFFFFFFFF
      s3 += r
      hist[ ( r >> 24 ) + 1 ]++

      xs32 := xs32 ^^ ( ( xs32 << 13 ) & 0xFFFFFFFF )
      xs32 := xs32 ^^ ( xs32 >> 17 )
      xs32 := xs32 ^^ ( ( xs32 << 5 ) & 0xFFFFFFFF )
      s4 += xs32
      hist[ ( xs32 >> 24 ) + 1 ]++
   NEXT
   FOR i := 1 TO 256
      check += hist[ i ] * i
   NEXT
   ok := s1 == 32217797789519680 .AND. s2 == 64428429046975605 .AND. s3 == 64423232821249815 .AND. ;
         s4 == 64428119329011698 .AND. check == 15421035017 .AND. Abs( fsum - 15002581.187856009 ) < 0.000001
   ? "random_massivo", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
