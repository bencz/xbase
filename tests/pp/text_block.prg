// TEXT ... ENDTEXT: a QOut() of each line as written, a string literal whose delimiter fits its text (all three kinds
// of quotes give a concatenation), then QQOut(); a command written as `text <lineFunc>, <endFunc>` takes the lines the
// same way, and one with only <lineFunc> ends with no call.
TEXT
plain
  "double" and 'single'
"'] all three
ENDTEXT
text OutStd, QQOut
by hand
endtext
text OutErr
only lines
ENDTEXT
