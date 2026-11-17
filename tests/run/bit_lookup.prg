// The bitwise AND of two 16-bit numbers from a 256-entry table of the ANDs of two 4-bit numbers, a nibble at a time,
// and IOR and XOR made from it by macros (a port of a C program): the table is built by function-like #defines nested
// three deep (C4, L, L4: 4, 16, 64 elements each, four of L4), a STATIC's initializer, which must fold to constants;
// #undef drops them. Every pair a, b in 0 .. 499 is checked against the operators &, | and ^^.
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
   LOCAL a, b
   ? Len( s_aAnd ), s_aAnd[ 256 ], s_aAnd[ 16 * 5 + 3 + 1 ]
   FOR a := 0 TO 499
      FOR b := 0 TO 499
         IF AND( a, b ) != ( a & b )
            ? "AND error", a, b
            RETURN
         ENDIF
         IF IOR( a, b ) != ( a | b )
            ? "IOR error", a, b
            RETURN
         ENDIF
         IF XOR( a, b ) != ( a ^^ b )
            ? "XOR error", a, b
            RETURN
         ENDIF
      NEXT
   NEXT
   ? "ok"
RETURN

FUNCTION AND( a, b )
   LOCAL r := 0, i
   FOR i := 0 TO 15 STEP 4
      r := Int( r / 16 ) + s_aAnd[ ( a % 16 ) * 16 + ( b % 16 ) + 1 ] * 4096
      a := Int( a / 16 )
      b := Int( b / 16 )
   NEXT
RETURN r
