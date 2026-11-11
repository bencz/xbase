// Extended codeblocks: a line that ends with {| params | takes the lines up to the one starting with }, whose
// statements are preprocessed on their own; a command around the codeblock (?) sees it whole; codeblocks nest, and a
// closing line may open another one. A } line in a skipped #ifdef part closes nothing, and a stringified codeblock
// gives its statements.
? Eval( {| a |
   ? "inside", a
   IF a > 1
      RETURN {||
         RETURN a * 2
      }
   ENDIF
   }, 7 ), "after"
b := {| x, y |
   RETURN x + y
}, c := {||
   ? "second"
}
d := {||
#ifdef NOT_DEFINED
   e := {||
      ? 1
   }
#endif
   RETURN 2
}
#xtranslate STRIZE( <x> ) => <"x">
? STRIZE( {||
   RETURN 1
} )
