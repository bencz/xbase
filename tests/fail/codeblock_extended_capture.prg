// A codeblock inside an extended one cannot capture its LOCALs (as in the reference).
PROCEDURE Main()
   LOCAL b := {| k |
      LOCAL j := k
      RETURN {|| j }
   }
RETURN
