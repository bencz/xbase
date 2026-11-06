// The bit operators are single tokens, also in #define bodies and in rule results, and work in #if; a marker
// written right next to an operator made of < > = keeps its brackets.
#define MASK      0xFF << 8
#define LOW( x )  ( ( x ) & 0xFF )
#xtranslate ROTL32( <n>, <c> ) => ( ( <n> << <c> | <n> >> ( 32 - <c> ) ) & 0xFFFFFFFF )
#xcommand SWAP BITS OF <v> => <v> := ~<v> ^^ MASK
#xtranslate SH( <n>, <c> ) => ( <n>>><c> )
#xtranslate SHQ( <n>, <c> ) => ( <(n)>>><"c"> )
#xcommand SHIFT <v> BY <c> => <v>>>=<c>
a := MASK | LOW( b ) ^^ ~c
a &= 1 ; a |= 2 ; a ^^= 3 ; a <<= 4 ; a >>= 5
r := ROTL32( x, 4 )
SWAP BITS OF y
b := {| p | p | 1 } ; c := a<<1>>2
d := SH( x, 2 ) + SHQ( y, 3 )
SHIFT z BY 4
#define FLAGS 6
#if ( FLAGS & 4 ) != 0 .AND. ~0 == -1 .AND. 1 << 3 == 8 .AND. -16 >> 2 == -4 .AND. ( 5 ^^ 1 | 8 ) == 12
ifok := 1
#else
ifok := 0
#endif
#if FLAGS & 1
odd := 1
#endif
