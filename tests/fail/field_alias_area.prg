// After FIELD->alias-> comes a field name, not ( expr ) as after an alias alone.
PROCEDURE Main()
   LOCAL x := FIELD->T->( RecNo() )
RETURN
