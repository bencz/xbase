/* The frames of DispBox() and @ ... BOX (docs/terminal.md): top left, top, top right, right, bottom right, bottom,
   bottom left, left; written in UTF-8, the default codepage (under another, DispBox( ..., 1 ) and ( ..., 2 ) draw the
   single and the double frames). */
#ifndef _BOX_CH
#define _BOX_CH

#define B_SINGLE        "┌─┐│┘─└│"
#define B_DOUBLE        "╔═╗║╝═╚║"
#define B_SINGLE_DOUBLE "╓─╖║╜─╙║"
#define B_DOUBLE_SINGLE "╒═╕│╛═╘│"

#endif
