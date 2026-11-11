// The languages of the runtime's texts, which SET LANGUAGE TO selects (English until then): the names of the days and
// of the months (CDoW(), CMonth()) and the descriptions of the errors the runtime raises.
#include "error.ch"
#include "language.ch"

// __LanguageTexts( cId ): the texts of the language cId (upper case: "EN", "PT", "ES") in the parts language.ch
// numbers, NIL for a language the runtime does not have. Each call makes them anew: the runtime keeps the current
// language's (__LanguageCurrent()).
FUNCTION __LanguageTexts( cId )
   SWITCH cId
   CASE "EN"
      RETURN { "EN", ;
         { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }, ;
         { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", ;
           "November", "December" }, ;
         { EG_ARG => "Argument error", EG_BOUND => "Bound error", EG_STROVERFLOW => "String overflow", ;
           EG_ZERODIV => "Zero divisor", EG_NOFUNC => "Undefined function", EG_NOMETHOD => "No exported method", ;
           EG_NOVARMETHOD => "No exported variable", EG_NOVAR => "Variable does not exist", ;
           EG_CREATE => "Create error", EG_OPEN => "Open error", EG_WRITE => "Write error", ;
           EG_SYNTAX => "Syntax error" }, ;
         { EG_NOMETHOD => { 41 => "Scope violation (hidden)", 42 => "Scope violation (protected)" } } }
   CASE "PT"
      RETURN { "PT", ;
         { "Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado" }, ;
         { "Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", ;
           "Novembro", "Dezembro" }, ;
         { EG_ARG => "Erro de argumento", EG_BOUND => "Erro de limite", EG_STROVERFLOW => "Estouro de string", ;
           EG_ZERODIV => "Divisão por zero", EG_NOFUNC => "Função não definida", ;
           EG_NOMETHOD => "Método não exportado", EG_NOVARMETHOD => "Variável não exportada", ;
           EG_NOVAR => "Variável não existe", EG_CREATE => "Erro de criação", EG_OPEN => "Erro de abertura", ;
           EG_WRITE => "Erro de gravação", EG_SYNTAX => "Erro de sintaxe" }, ;
         { EG_NOMETHOD => { 41 => "Violação de escopo (oculto)", 42 => "Violação de escopo (protegido)" } } }
   CASE "ES"
      RETURN { "ES", ;
         { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" }, ;
         { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", ;
           "Noviembre", "Diciembre" }, ;
         { EG_ARG => "Error de argumento", EG_BOUND => "Error de límite", ;
           EG_STROVERFLOW => "Desbordamiento de cadena", EG_ZERODIV => "División por cero", ;
           EG_NOFUNC => "Función no definida", EG_NOMETHOD => "Método no exportado", ;
           EG_NOVARMETHOD => "Variable no exportada", ;
           EG_NOVAR => "La variable no existe", EG_CREATE => "Error de creación", EG_OPEN => "Error de apertura", ;
           EG_WRITE => "Error de escritura", EG_SYNTAX => "Error de sintaxis" }, ;
         { EG_NOMETHOD => { 41 => "Violación de ámbito (oculto)", 42 => "Violación de ámbito (protegido)" } } }
   ENDSWITCH
RETURN NIL
