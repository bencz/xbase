// AWAIT is a prefix operator where an operand follows it: a command's list marker takes `AWAIT f( x )` as one
// expression. AWAIT followed by `(` is a call of Await(), and so ends nothing either.
? AWAIT F( 1 ), AWAIT t, AWAIT ::Get(), AWAIT &cTask, x
? Await( 2 ) + await (3), AWAIT {| | 1 }
STORE AWAIT oTask TO y
// AWAIT followed by a word of the command's rule is a name there.
INDEX ON await TO foo
REPLACE await WITH 1
