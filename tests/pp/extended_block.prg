// Extended codeblocks: a line that ends with {| params | takes the lines up to the one starting with }, whose
// statements are preprocessed on their own; a command around the codeblock (?) sees it whole; codeblocks nest, and a
// closing line may open another one.
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
