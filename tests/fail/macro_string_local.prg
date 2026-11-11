// `&name` in a string stands for a memory variable's text: naming a LOCAL there is an error, since the
// substitution would never see it.
PROCEDURE Main()
   LOCAL cName := "x"
   ? "Hello &cName."
RETURN
