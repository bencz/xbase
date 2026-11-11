// WITH OBJECT, compared with the reference: :name reads, assigns (also :name++ and :name += x) and calls on the
// object, evaluated once (assigning the variable it came from changes nothing), nested ones (the innermost wins), a
// codeblock made inside and evaluated there, and END WITH. Deliberate difference: a codeblock captures the object the
// statement holds, also when it is evaluated after END (the reference sends to whatever WITH OBJECT runs then, and
// fails outside one).
PROCEDURE Main()
   LOCAL o := TP():New(), b, p := TP():New(), a := { TP():New(), TP():New() }, x
   WITH OBJECT o
      :x := 10
      :y := :x * 2
      ? "sends:", :x, :y, :Sum(), :ClassName()
      b := {|| :x + 1 }
      ? "block:", Eval( b )
      WITH OBJECT p
         :x := 5
         ? "inner:", :x, :Sum()
      END
      ? "outer again:", :x
      o := p
      ? "after reassign:", :x
      :x++
      :y += 100
      ? "compound:", :x, :y
   END WITH
   ? "objects:", o:x, p:x
   ? "block after END:", Eval( b )
   FOR EACH x IN a
      WITH OBJECT x
         :x := x:Seven()
      END
   NEXT
   ? "loop:", a[ 1 ]:x, a[ 2 ]:x
   b := {}
   a[ 1 ]:x := "one"
   a[ 2 ]:x := "two"
   FOR EACH x IN a
      WITH OBJECT x
         AAdd( b, {|| :x } )
      END
   NEXT
   ? "blocks made in a loop:", Eval( b[ 1 ] ), Eval( b[ 2 ] )
RETURN

CLASS TP
   VAR x INIT 0
   VAR y INIT 0
   METHOD Sum() INLINE ::x + ::y
   METHOD Seven() INLINE 7
ENDCLASS
