# Libraries

The libraries xbase provides, in `libs/<name>/`, built and installed with the compiler; a program uses one with
`libs = <name>` in its project file or `--lib=<name>` ([projects.md](projects.md)). Each is classes only: a function
that is not a method is a `STATIC` helper of its module.

## cli

`CommandLine` reads a program's command line: the options it declares (`--name value`, `--name=value`, `-x value`),
its flags (`--name`, `-x`) and its positional arguments, and makes the help text from the declarations
(`libs/cli/cli.prg`; `samples/wordfreq` uses it).

```
PROCEDURE Main( ... )
   LOCAL oCmd := CommandLine():New( "wordfreq", "Shows the most frequent words of text files." )
   oCmd:AddOption( "top", "n", "how many words to show", "10" )
   oCmd:AddFlag( "ignore-case", "i", "counts words without letter case" )
   oCmd:AddArgument( "file", "a text file", .T. )
   IF ! oCmd:Parse( { ... } )
      ...   // oCmd:HelpRequested(), oCmd:ErrorText(), oCmd:Usage()
```

| Method | Does |
|---|---|
| `New( cProgram, [cDescription] )` | the program's name and description, for the help text |
| `AddOption( cName, [cShort], [cHelp], [cDefault] )` | an option taking a value: `--name value`, `--name=value` (an empty value too), and `-x value` when `cShort` is its letter; `cDefault` is its value when not given |
| `AddFlag( cName, [cShort], [cHelp] )` | a flag: `--name`, `-x`; giving it a value is an error |
| `AddArgument( cName, [cHelp], [lRequired] )` | a positional argument, taken in the order declared |
| `Parse( aArgs )` | reads the arguments (the strings a program's `Main( ... )` receives, `{ ... }`); `.F.` when they are wrong or `--help` (`-h`) is among them. Each call starts afresh |
| `Value( cName )` | an option's value (its default when not given), a flag's `.T.` or `.F.`, a positional argument's value (`NIL` when not given); `NIL` for a name not declared |
| `IsSet( cName )` | whether the option, flag or argument was given |
| `Rest()` | the positional arguments after the declared ones (a copy) |
| `ErrorText()` | why `Parse()` failed: `unknown option --x`, `the flag --x takes no value`, `the option --x needs a value`, `missing <name>` |
| `HelpRequested()` | whether `Parse()` stopped at `--help` or `-h` |
| `Usage()` | the help text: the usage line, the description, the arguments and options with their help, one a line (separated by `Chr( 10 )`) |

An option given twice keeps the last value. After `--` every argument is positional, whatever it starts with; a lone
`-` is positional. Options and positional arguments may come in any order. An option's value is the next argument
whatever it starts with (`--top -5`). The names of the options, flags and arguments are one set: a name declared twice
shares one value.
