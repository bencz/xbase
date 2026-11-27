// Parameters declared AS INTEGER whose values the code does not know (any caller's) are guarded against integers
// alone where they enter the loop (subst.check guard 4), not against every number (12): AS is the guards' hint.
FUNCTION Sum( n AS INTEGER, k AS INTEGER )
   LOCAL i, s := 0
   FOR i := 1 TO n
      s += i * k - n
   NEXT
RETURN s
