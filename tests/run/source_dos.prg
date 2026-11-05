// A ";" followed by a block comment continues the line, also when the comment crosses lines; Ctrl-Z (written by
// DOS editors at the end of files, and here inside a line too) is a blank.
PROCEDURE Main()
   LOCAL x := 1 + ; /* a comment */
      2
   LOCAL y := 10 + ; /* a comment
      over two lines */ 5
   ? x, y
   ? "end"
   RETURN
