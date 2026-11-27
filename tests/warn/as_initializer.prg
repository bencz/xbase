// A LOCAL declared AS NUMERIC whose initializer is a string literal: a warning (AS is a hint, the value is kept).
PROCEDURE Main()
   LOCAL n AS NUMERIC := "x"
   ? n
RETURN
