// Values kept outside the frame (numbers, logicals) carried around loops: swapped through a loop's back edge, rotated,
// passed to themselves, and mixed with values in frame slots (strings).
PROCEDURE Main()
   LOCAL a := 1, b := 2.5, c := 3, t, i, l1 := .T., l2 := .F., s1 := "x", s2 := "y"
   FOR i := 1 TO 5
      t := a
      a := b
      b := t
      t := l1
      l1 := l2
      l2 := t
      t := s1
      s1 := s2
      s2 := t
      a := a
   NEXT
   ? a, b, l1, l2, s1, s2
   FOR i := 1 TO 4
      t := a
      a := b
      b := c
      c := t
   NEXT
   ? a, b, c
   i := 0
   DO WHILE i < 3
      a := IIf( i % 2 == 0, b, a + 1 )
      b := a * 2
      i++
   ENDDO
   ? a, b, i
   RETURN
