// Transform() as programs commonly use it: integers, decimals, thousands separators, $ and * fills, signs, strings
// (@!, @S, @B, @R), dates, logicals, rounding of halves, values too wide (stars), each checked against its expected
// text (the result is as wide as the template; $ and * fill on the left; - and + in a template are literals; a
// logical without a template is T or F; @S cuts and does not pad; @C and @X add " CR" and " DB"). Every check passes.


STATIC nTotal := 0
STATIC nPassed := 0
STATIC nFailed := 0

PROCEDURE Main()


   ? "=============================================="
   ? "     TESTE COMPLETO - TRANSFORM()"
   ? "=============================================="
   ?

   TestIntegers()
   TestDecimals()
   TestThousands()
   TestCurrency()
   TestSigns()
   TestStrings()
   TestDates()
   TestLogical()
   TestSpecialMasks()
   TestRounding()
   TestBoundaries()
   TestRepeatedMasks()
   TestCurrencyAdvanced()
   TestBrazilianCurrency()
   TestCurrencyFill()
   TestFinancialSigns()
   TestFinancialRounding()
   TestCurrencyOverflow()
   TestPricesAndPercentages()

   ?
   ? "=============================================="
   ? "               RESULTADO FINAL"
   ? "=============================================="
   ? "Total de testes :", nTotal
   ? "Aprovados       :", nPassed
   ? "Reprovados      :", nFailed
   ? "=============================================="

   IF nFailed == 0
      ? "TODOS OS TESTES PASSARAM!"
   ELSE
      ? "EXISTEM TESTES COM FALHA!"
   ENDIF

   ?

RETURN

STATIC PROCEDURE Check( cName, xValue, xExpected )

   LOCAL lOK := ( ValType( xValue ) == ValType( xExpected ) )

   IF lOK
      lOK := ( xValue == xExpected )
   ENDIF

   nTotal++

   IF lOK
      nPassed++
      ? "[ OK ] ", cName
   ELSE
      nFailed++
      ? "[FAIL] ", cName
      ? "       Esperado:", xExpected
      ? "       Obtido  :", xValue
   ENDIF

RETURN

STATIC PROCEDURE TestIntegers()

   ? " "
   ? "--- NUMEROS INTEIROS ---"

   Check( "Inteiro simples", ;
      Transform( 123, "999" ), ;
      "123" )

   Check( "Inteiro com espacos", ;
      Transform( 12, "99999" ), ;
      "   12" )

   Check( "Inteiro zero", ;
      Transform( 0, "999" ), ;
      "  0" )

   Check( "Inteiro negativo", ;
      Transform( -123, "9999" ), ;
      "-123" )

   Check( "Mascara com asteriscos", ;
      Transform( 123, "***9" ), ;
      "*123" )

   Check( "Mascara com preenchimento", ;
      Transform( 12, "@Z 99999" ), ;
      "   12" )

RETURN

STATIC PROCEDURE TestDecimals()

   ? " "
   ? "--- NUMEROS DECIMAIS ---"

   Check( "Duas casas decimais", ;
      Transform( 123.45, "999.99" ), ;
      "123.45" )

   Check( "Uma casa decimal", ;
      Transform( 123.45, "999.9" ), ;
      "123.5" )

   Check( "Tres casas decimais", ;
      Transform( 123.456, "999.999" ), ;
      "123.456" )

   Check( "Valor decimal zero", ;
      Transform( 0.00, "999.99" ), ;
      "  0.00" )

   Check( "Valor decimal negativo", ;
      Transform( -12.34, "999.99" ), ;
      "-12.34" )

   Check( "Decimal com largura maior", ;
      Transform( 12.34, "999999.9999" ), ;
      "    12.3400" )

RETURN

STATIC PROCEDURE TestThousands()

   ? " "
   ? "--- SEPARADORES DE MILHARES ---"

   Check( "Milhar", ;
      Transform( 1234, "9,999" ), ;
      "1,234" )

   Check( "Milhoes", ;
      Transform( 1234567, "9,999,999" ), ;
      "1,234,567" )

   Check( "Milhar decimal", ;
      Transform( 1234.56, "9,999.99" ), ;
      "1,234.56" )

   Check( "Milhar com zero", ;
      Transform( 0, "9,999" ), ;
      "    0" )

RETURN

STATIC PROCEDURE TestCurrency()

   ? " "
   ? "--- SIMBOLOS MONETARIOS ---"

   Check( "Simbolo monetario", ;
      Transform( 123.45, "$999.99" ), ;
      "$123.45" )

   Check( "Simbolo monetario com milhar", ;
      Transform( 1234.56, "$9,999.99" ), ;
      "$1,234.56" )

   Check( "Simbolo monetario com espacos", ;
      Transform( 12.5, "$999.99" ), ;
      "$ 12.50" )

RETURN

STATIC PROCEDURE TestSigns()

   ? " "
   ? "--- SINAIS ---"

   Check( "Negativo", ;
      Transform( -123, "9999" ), ;
      "-123" )

   Check( "Positivo", ;
      Transform( 123, "9999" ), ;
      " 123" )

   Check( "Mascara com sinal", ;
      Transform( -123, "999-" ), ;
      "***-" )

   Check( "Mascara com sinal positivo", ;
      Transform( 123, "999+" ), ;
      "123+" )

RETURN

STATIC PROCEDURE TestStrings()

   ? " "
   ? "--- STRINGS ---"

   Check( "String simples", ;
      Transform( "HARBOUR", "" ), ;
      "HARBOUR" )

   Check( "String em minusculo", ;
      Transform( "harbour", "@!" ), ;
      "HARBOUR" )

   Check( "String em maiusculo", ;
      Transform( "Clipper", "@!" ), ;
      "CLIPPER" )

   Check( "String sem alteracao", ;
      Transform( "Harbour", "@!" ), ;
      "HARBOUR" )

   Check( "String com preenchimento", ;
      Transform( "TESTE", "@S10" ), ;
      "TESTE" )

   Check( "String com espacos", ;
      Transform( "  ABC  ", "" ), ;
      "  ABC  " )

   Check( "String vazia", ;
      Transform( "", "" ), ;
      "" )

RETURN

STATIC PROCEDURE TestDates()

   LOCAL dDate := SToD( "20261113" )

   ? " "
   ? "--- DATAS ---"

   Check( "Data padrao", ;
      Transform( dDate, "" ), ;
      DToC( dDate ) )

   Check( "Data com mascara", ;
      Transform( dDate, "@D" ), ;
      DToC( dDate ) )

   Check( "Data vazia", ;
      Transform( CToD( "" ), "" ), ;
      "  /  /  " )

RETURN

STATIC PROCEDURE TestLogical()

   ? " "
   ? "--- VALORES LOGICOS ---"

   Check( "Logico verdadeiro", ;
      Transform( .T., "" ), ;
      "T" )

   Check( "Logico falso", ;
      Transform( .F., "" ), ;
      "F" )

RETURN

STATIC PROCEDURE TestSpecialMasks()

   ? " "
   ? "--- MASCARAS ESPECIAIS ---"

   Check( "Maiusculas", ;
      Transform( "harbour", "@!" ), ;
      "HARBOUR" )

   Check( "Sem espacos a esquerda", ;
      Transform( "   TESTE", "@B" ), ;
      "TESTE   " )

   Check( "String com tamanho", ;
      Transform( "ABC", "@S8" ), ;
      "ABC" )

   Check( "Mascara @R", ;
      Transform( "123456", "@R 999-999" ), ;
      "123-456" )

RETURN

STATIC PROCEDURE TestRounding()

   ? " "
   ? "--- ARREDONDAMENTO ---"

   Check( "Arredonda para cima", ;
      Transform( 12.345, "999.99" ), ;
      " 12.35" )

   Check( "Arredonda para baixo", ;
      Transform( 12.344, "999.99" ), ;
      " 12.34" )

   Check( "Arredondamento inteiro", ;
      Transform( 12.8, "999" ), ;
      " 13" )

   Check( "Arredondamento negativo", ;
      Transform( -12.345, "999.99" ), ;
      "-12.35" )

RETURN

STATIC PROCEDURE TestBoundaries()

   ? " "
   ? "--- LIMITES ---"

   Check( "Numero zero", ;
      Transform( 0, "99999" ), ;
      "    0" )

   Check( "Numero pequeno", ;
      Transform( 0.01, "999.99" ), ;
      "  0.01" )

   Check( "Numero grande", ;
      Transform( 999999, "9999999" ), ;
      " 999999" )

   Check( "Numero negativo grande", ;
      Transform( -999999, "9999999" ), ;
      "-999999" )

   Check( "Numero com overflow", ;
      Transform( 123456, "999" ), ;
      "***" )

RETURN

STATIC PROCEDURE TestRepeatedMasks()

   LOCAL nI

   ? " "
   ? "--- TESTES DE REPETICAO ---"

   FOR nI := 1 TO 1000

      IF Transform( 123.45, "999.99" ) != "123.45"
         Check( "Repeticao " + LTrim( Str( nI ) ), ;
            Transform( 123.45, "999.99" ), ;
            "123.45" )
         EXIT
      ENDIF

   NEXT

   Check( "1000 execucoes sem alteracao", ;
      Transform( 123.45, "999.99" ), ;
      "123.45" )

RETURN

STATIC PROCEDURE TestCurrencyAdvanced()

   ? " "
   ? "--- MASCARAS MONETARIAS AVANCADAS ---"

   Check( "Moeda simples", ;
      Transform( 123.45, "$999.99" ), ;
      "$123.45" )

   Check( "Moeda com milhar", ;
      Transform( 1234.56, "$9,999.99" ), ;
      "$1,234.56" )

   Check( "Moeda com milhoes", ;
      Transform( 1234567.89, "$9,999,999.99" ), ;
      "$1,234,567.89" )

   Check( "Moeda zero", ;
      Transform( 0.00, "$999.99" ), ;
      "$  0.00" )

   Check( "Moeda negativa", ;
      Transform( -123.45, "$999.99" ), ;
      "-123.45" )

   Check( "Moeda negativa com milhar", ;
      Transform( -1234.56, "$9,999.99" ), ;
      "-1,234.56" )

   Check( "Moeda com tres casas", ;
      Transform( 123.456, "$999.999" ), ;
      "$123.456" )

   Check( "Moeda com quatro casas", ;
      Transform( 123.4567, "$999.9999" ), ;
      "$123.4567" )

RETURN

STATIC PROCEDURE TestBrazilianCurrency()

   ? " "
   ? "--- FORMATACAO BRASILEIRA ---"

   SET DECIMALS TO 2
   SET DATE BRITISH

   Check( "Valor com duas casas", ;
      Transform( 1234.56, "9,999.99" ), ;
      "1,234.56" )

   Check( "Valor com centavos zero", ;
      Transform( 1234.00, "9,999.99" ), ;
      "1,234.00" )

   Check( "Valor com centavos pequenos", ;
      Transform( 0.05, "999.99" ), ;
      "  0.05" )

   Check( "Valor com centavos grandes", ;
      Transform( 0.99, "999.99" ), ;
      "  0.99" )

   Check( "Valor negativo brasileiro", ;
      Transform( -1234.56, "9,999.99" ), ;
      "*****.**" )

RETURN

STATIC PROCEDURE TestCurrencyFill()

   ? " "
   ? "--- CIFRAO E PREENCHIMENTO ---"

   Check( "Cifrao com largura", ;
      Transform( 12.34, "$9999.99" ), ;
      "$  12.34" )

   Check( "Cifrao com asteriscos", ;
      Transform( 12.34, "$***9.99" ), ;
      "$**12.34" )

   Check( "Preenchimento com asteriscos", ;
      Transform( 12.34, "****9.99" ), ;
      "***12.34" )

   Check( "Mascara monetaria larga", ;
      Transform( 123.45, "$999999.99" ), ;
      "$   123.45" )

   Check( "Mascara monetaria com zero", ;
      Transform( 0, "$9999.99" ), ;
      "$   0.00" )

RETURN

STATIC PROCEDURE TestFinancialSigns()

   ? " "
   ? "--- SINAIS FINANCEIROS ---"

   Check( "Negativo simples", ;
      Transform( -123.45, "999.99" ), ;
      "***.**" )

   Check( "Negativo com cifrao", ;
      Transform( -123.45, "$999.99" ), ;
      "-123.45" )

   Check( "Positivo com @C", ;
      Transform( 123.45, "@C 999.99" ), ;
      "123.45 CR" )

   Check( "Negativo com @X", ;
      Transform( -123.45, "@X 999.99" ), ;
      "123.45 DB" )

   Check( "Zero com @Z", ;
      Transform( 0, "@Z 999.99" ), ;
      "      " )

   Check( "Negativo com parenteses", ;
      Transform( -123.45, "@( 999.99" ), ;
      "(**.**)" )

RETURN

STATIC PROCEDURE TestFinancialRounding()

   ? " "
   ? "--- ARREDONDAMENTO FINANCEIRO ---"

   Check( "Arredondamento 1", ;
      Transform( 10.125, "999.99" ), ;
      " 10.13" )

   Check( "Arredondamento 2", ;
      Transform( 10.124, "999.99" ), ;
      " 10.12" )

   Check( "Arredondamento 3", ;
      Transform( 99.999, "999.99" ), ;
      "100.00" )

   Check( "Arredondamento negativo 1", ;
      Transform( -10.125, "999.99" ), ;
      "-10.13" )

   Check( "Arredondamento negativo 2", ;
      Transform( -10.124, "999.99" ), ;
      "-10.12" )

   Check( "Casas decimais extras", ;
      Transform( 123.456789, "999.999999" ), ;
      "123.456789" )

   Check( "Duas casas com valor inteiro", ;
      Transform( 100, "999.99" ), ;
      "100.00" )

RETURN

STATIC PROCEDURE TestCurrencyOverflow()

   ? " "
   ? "--- OVERFLOW MONETARIO ---"

   Check( "Valor maior que mascara", ;
      Transform( 123456.78, "999.99" ), ;
      "***.**" )

   Check( "Milhoes", ;
      Transform( 9999999.99, "9,999,999.99" ), ;
      "9,999,999.99" )

   Check( "Valor negativo grande", ;
      Transform( -999999.99, "999999.99" ), ;
      "******.**" )

   Check( "Valor proximo do limite", ;
      Transform( 999.99, "999.99" ), ;
      "999.99" )

   Check( "Valor acima do limite", ;
      Transform( 1000.00, "999.99" ), ;
      "***.**" )

RETURN

STATIC PROCEDURE TestPricesAndPercentages()

   ? " "
   ? "--- PRECOS E PERCENTUAIS ---"

   Check( "Preco unitario", ;
      Transform( 9.99, "$999.99" ), ;
      "$  9.99" )

   Check( "Preco de produto", ;
      Transform( 1499.90, "$9,999.99" ), ;
      "$1,499.90" )

   Check( "Preco alto", ;
      Transform( 99999.99, "$99,999.99" ), ;
      "$99,999.99" )

   Check( "Percentual", ;
      Transform( 12.5, "999.99" ), ;
      " 12.50" )

   Check( "Percentual negativo", ;
      Transform( -5.25, "999.99" ), ;
      " -5.25" )

   Check( "Taxa com quatro casas", ;
      Transform( 0.0525, "9.9999" ), ;
      "0.0525" )

RETURN
