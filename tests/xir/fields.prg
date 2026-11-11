// Fields: alias->name, FIELD->name and a FIELD declaration (with IN) are field.get and field.set with the alias's
// name; ( expr )->name passes the alias's value as the first operand; an undeclared name read is mv.field, assigned
// mv.set; alias->( expr ) calls __AREAEVAL with a codeblock.
FIELD QTY IN ORDERS

PROCEDURE Main()
   LOCAL c := "ORDERS"
   ? CUST->NAME, FIELD->NAME, QTY, ( c )->QTY, UNDECLARED
   CUST->NAME := "x"
   ( c )->QTY += 1
   QTY++
   UNDECLARED := 2
   ? CUST->( RecNo() )
RETURN
