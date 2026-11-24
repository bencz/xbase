// Codeblocks with parameters they do not share get a body for arguments without references (NAME.plain), which their
// direct entry runs; the blocks nested in it are those of the block's own body (three levels each with a parameter of
// its own make three blocks and three plain bodies, not a doubling per level). A block whose parameter is captured,
// one with none and one reading its arguments as received (PCount()) get none.
STATIC s_a := {}

FUNCTION Main()
   LOCAL n := 1
RETURN { {| a | AAdd( s_a, a ), {| b | AAdd( s_a, b ), {| c | c + n } } }, {| p | {|| p } }, {|| n }, ;
         {| x | PCount() + x } }
