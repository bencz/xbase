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
           EG_NUMOVERFLOW => "Numeric overflow", EG_ZERODIV => "Zero divisor", EG_NUMERR => "Numeric error", ;
           EG_SYNTAX => "Syntax error", EG_COMPLEXITY => "Operation too complex", EG_MEM => "Memory low", ;
           EG_NOFUNC => "Undefined function", EG_NOMETHOD => "No exported method", ;
           EG_NOVAR => "Variable does not exist", EG_NOALIAS => "Alias does not exist", ;
           EG_NOVARMETHOD => "No exported variable", EG_BADALIAS => "Illegal characters in alias", ;
           EG_DUPALIAS => "Alias already in use", EG_CREATE => "Create error", EG_OPEN => "Open error", ;
           EG_CLOSE => "Close error", EG_READ => "Read error", EG_WRITE => "Write error", EG_PRINT => "Print error", ;
           EG_UNSUPPORTED => "Operation not supported", EG_LIMIT => "Limit exceeded", ;
           EG_CORRUPTION => "Corruption detected", EG_DATATYPE => "Data type error", ;
           EG_DATAWIDTH => "Data width error", EG_NOTABLE => "Workarea not in use", ;
           EG_NOORDER => "Workarea not indexed", EG_SHARED => "Exclusive required", EG_UNLOCKED => "Lock required", ;
           EG_READONLY => "Write not allowed", EG_APPENDLOCK => "Append lock failed", EG_LOCK => "Lock Failure" }, ;
         { EG_NOMETHOD => { 41 => "Scope violation (hidden)", 42 => "Scope violation (protected)" } } }
   CASE "PT"
      RETURN { "PT", ;
         { "Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado" }, ;
         { "Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", ;
           "Novembro", "Dezembro" }, ;
         { EG_ARG => "Erro de argumento", EG_BOUND => "Erro de limite", EG_STROVERFLOW => "Estouro de string", ;
           EG_NUMOVERFLOW => "Estouro numérico", EG_ZERODIV => "Divisão por zero", EG_NUMERR => "Erro numérico", ;
           EG_SYNTAX => "Erro de sintaxe", EG_COMPLEXITY => "Operação complexa demais", ;
           EG_MEM => "Memória insuficiente", EG_NOFUNC => "Função não definida", ;
           EG_NOMETHOD => "Método não exportado", EG_NOVAR => "Variável não existe", EG_NOALIAS => "Alias não existe", ;
           EG_NOVARMETHOD => "Variável não exportada", EG_BADALIAS => "Caracteres inválidos no alias", ;
           EG_DUPALIAS => "Alias já em uso", EG_CREATE => "Erro de criação", EG_OPEN => "Erro de abertura", ;
           EG_CLOSE => "Erro de fechamento", EG_READ => "Erro de leitura", EG_WRITE => "Erro de gravação", ;
           EG_PRINT => "Erro de impressão", EG_UNSUPPORTED => "Operação não suportada", EG_LIMIT => "Limite excedido", ;
           EG_CORRUPTION => "Corrupção detectada", EG_DATATYPE => "Erro de tipo de dado", ;
           EG_DATAWIDTH => "Erro de tamanho de dado", EG_NOTABLE => "Área de trabalho não está em uso", ;
           EG_NOORDER => "Área de trabalho sem índice", EG_SHARED => "Uso exclusivo necessário", ;
           EG_UNLOCKED => "Bloqueio necessário", EG_READONLY => "Gravação não permitida", ;
           EG_APPENDLOCK => "Falha no bloqueio para inclusão", EG_LOCK => "Falha de bloqueio" }, ;
         { EG_NOMETHOD => { 41 => "Violação de escopo (oculto)", 42 => "Violação de escopo (protegido)" } } }
   CASE "ES"
      RETURN { "ES", ;
         { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" }, ;
         { "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", ;
           "Noviembre", "Diciembre" }, ;
         { EG_ARG => "Error de argumento", EG_BOUND => "Error de límite", ;
           EG_STROVERFLOW => "Desbordamiento de cadena", EG_NUMOVERFLOW => "Desbordamiento numérico", ;
           EG_ZERODIV => "División por cero", EG_NUMERR => "Error numérico", EG_SYNTAX => "Error de sintaxis", ;
           EG_COMPLEXITY => "Operación demasiado compleja", EG_MEM => "Memoria insuficiente", ;
           EG_NOFUNC => "Función no definida", EG_NOMETHOD => "Método no exportado", ;
           EG_NOVAR => "La variable no existe", EG_NOALIAS => "El alias no existe", ;
           EG_NOVARMETHOD => "Variable no exportada", EG_BADALIAS => "Caracteres no válidos en el alias", ;
           EG_DUPALIAS => "El alias ya está en uso", EG_CREATE => "Error de creación", EG_OPEN => "Error de apertura", ;
           EG_CLOSE => "Error de cierre", EG_READ => "Error de lectura", EG_WRITE => "Error de escritura", ;
           EG_PRINT => "Error de impresión", EG_UNSUPPORTED => "Operación no soportada", ;
           EG_LIMIT => "Límite excedido", EG_CORRUPTION => "Corrupción detectada", ;
           EG_DATATYPE => "Error de tipo de dato", EG_DATAWIDTH => "Error de ancho de dato", ;
           EG_NOTABLE => "Área de trabajo no en uso", EG_NOORDER => "Área de trabajo sin índice", ;
           EG_SHARED => "Se requiere uso exclusivo", EG_UNLOCKED => "Se requiere bloqueo", ;
           EG_READONLY => "Escritura no permitida", EG_APPENDLOCK => "Falló el bloqueo para añadir", ;
           EG_LOCK => "Falló el bloqueo" }, ;
         { EG_NOMETHOD => { 41 => "Violación de ámbito (oculto)", 42 => "Violación de ámbito (protegido)" } } }
   ENDSWITCH
RETURN NIL
