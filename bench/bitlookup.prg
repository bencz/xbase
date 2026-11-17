// The bitwise AND from a table of nibbles (tests/run/bit_lookup.prg at full size): every pair a, b in 0 .. 999, AND,
// IOR and XOR made of calls to an xBase function with a loop of 4 passes, checked against &, | and ^^.
#define XOR(a,b) (a + b - 2*AND(a,b))
#define IOR(a,b) XOR(XOR(a,b),AND(a,b))

#define C4(a,b) ((a)&(b)), ((a)&(b+1)), ((a)&(b+2)), ((a)&(b+3))
#define L(a) C4(a,0), C4(a,4), C4(a,8), C4(a,12)
#define L4(a) L(a), L(a+1), L(a+2), L(a+3)
STATIC s_aAnd := { L4(0), L4(4), L4(8), L4(12) }
#undef C4
#undef L
#undef L4

PROCEDURE Main()
   LOCAL a, b, t := ElapsedMs()
   FOR a := 0 TO 999
      FOR b := 0 TO 999
         IF AND( a, b ) != ( a & b )
            ? "bitlookup", ElapsedMs() - t, "WRONG"
            RETURN
         ENDIF
         IF IOR( a, b ) != ( a | b )
            ? "bitlookup", ElapsedMs() - t, "WRONG"
            RETURN
         ENDIF
         IF XOR( a, b ) != ( a ^^ b )
            ? "bitlookup", ElapsedMs() - t, "WRONG"
            RETURN
         ENDIF
      NEXT
   NEXT
   ? "bitlookup", ElapsedMs() - t, "ok"
RETURN

FUNCTION AND( a, b )
   LOCAL r := 0, i
   FOR i := 0 TO 15 STEP 4
      r := Int( r / 16 ) + s_aAnd[ ( a % 16 ) * 16 + ( b % 16 ) + 1 ] * 4096
      a := Int( a / 16 )
      b := Int( b / 16 )
   NEXT
RETURN r
