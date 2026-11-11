# Terminal (planned)

The design of the screen, the keyboard and the mouse: everything a full-screen program uses (`@ SAY`, `CLS`, `Inkey`,
`SaveScreen`, `SetColor`, `Alert`, `AChoice`, `MemoEdit`, `GET`/`READ`, `TBrowse`). Nothing of it exists yet; this
document is the plan the work follows, and becomes the description of the code as parts of it are done.

Goals: Clipper's behavior on every platform; programs that adapt to the size of the terminal or window while they run
(`MaxRow()` and `MaxCol()` follow it, and a resize reaches the program as an event); exact text and colors in any
terminal in use today; output that tests can check; no library a program must carry along.

## Layers

1. **The screen model** (portable C, `runtime/src/term/`): a grid of cells, each a character (a Unicode code point,
   with its width: 1, or 2 for wide characters, whose second cell continues the first) and a color attribute
   (foreground and background, 16 colors each, as Clipper's); the cursor (position and shape, `SetCursor`); the current
   colors (`SetColor`: standard, enhanced, border, background, unselected). Every screen function of the language works
   on the model only, never on a driver: `DevPos`, `DevOut`, `DispOut`, `DispOutAt`, `Scroll`, `DispBox`, `SaveScreen`
   and `RestScreen` (a cells string, as Clipper's, opaque to programs), `SetPos`, `Row`, `Col`, `MaxRow`, `MaxCol`,
   `DispBegin`/`DispEnd`/`DispCount` (updates held until the outermost `DispEnd`), and `?`/`QOut` on the screen (at the
   cursor, scrolling the screen at its bottom). So every driver shows the same thing.
2. **Events** (portable C): one queue of keys (Clipper's `Inkey` codes), mouse events (Clipper 5.3's codes and event
   masks), resize events, the `KEYBOARD` buffer (`SET TYPEAHEAD`), `SET KEY` blocks; `Inkey( [nSeconds], [nMask] )`,
   `NextKey`, `LastKey`, `MRow`, `MCol` and the other mouse functions read it.
3. **Drivers**: the only code that touches the operating system's terminal or window. A driver opens and closes the
   device, tells its size, shows the cells that changed and the cursor, and gives events with a timeout. The interface
   is small, so drivers are easy to add.
4. **The library in xBase** (`runtime/src/rtl/*.prg`): `Alert`, `AChoice`, `MemoEdit`, the `GET` class and `READ`,
   `TBrowse` and `TBColumn`, `dbEdit`, menus: written over layers 1 and 2.

## Drivers

| Driver | Where | What |
|---|---|---|
| Stream | everywhere | No screen: output goes to standard output as a stream, as today (pipes, files, servers, tests of plain output). Screen functions keep the model and show nothing. |
| VT | Linux, macOS, BSD | xbase's own: the terminal in raw mode (`termios`) and the ECMA-48/xterm sequences: cursor addressing, 16 colors (`SGR`), the alternate screen, cursor shape and visibility, mouse (`SGR` mode 1006), bracketed paste; keys decoded from xterm's sequences with modifiers (Ctrl, Alt, Shift with arrows, Home, End, PgUp, PgDn, Ins, Del, F1 to F12). |
| Console | Windows | The console API: input records (`ReadConsoleInputW`: keys with their modifiers, mouse, buffer resize events) and cell output. |
| Dump | everywhere | For tests: the screen in memory, written as text (characters, then colors, row by row) when the program ends or on request; keys come from `KEYBOARD`. |
| Window | later, a library | A window of its own (Windows; X11 and Wayland), drawing the cells with a font; resizable, which reaches the program as a resize like a terminal's. |

The Stream, VT, Console and Dump drivers are part of the runtime (they need nothing outside the C library); the Window
driver is a library in `libs/` ([architecture.md](architecture.md#libraries-written-in-xbase)), which registers itself
with the terminal layer when a program links it. The driver is chosen when the program starts: the VT or Console driver
when standard input and output are a terminal, the Stream driver otherwise; a Clipper-style runtime switch on the
command line, which the program does not see, chooses another (`//TERM:STREAM`, `//TERM:VT`, `//TERM:CONSOLE`,
`//TERM:DUMP`). Never an environment variable.

### Why xbase's own VT driver and not ncurses

ncurses (6.6 here, with wide characters and extended colors) would do the screen output and decode keys from the
terminfo database. For Clipper programs it falls short where they depend on the terminal most, and it costs every
program a dependency:

- Keys: Clipper programs use Ctrl, Alt and Shift with the cursor keys and function keys (`K_CTRL_LEFT`, `K_ALT_X`,
  `K_SH_F3`, `K_CTRL_PGDN`, ...). ncurses gives the plain keys through terminfo, but the modified ones only as
  extended key names that differ from terminal to terminal, and Alt as an Escape prefix guessed with a delay. Our own
  decoder reads the modifier parameters of the xterm sequences, which every current terminal sends, without delays
  except for a lone Escape.
- Colors: Clipper's attributes are 16 foreground by 16 background colors, which the `SGR` sequences give directly;
  ncurses needs color pairs set up and managed.
- Dependency: every program would link `libncursesw` and need the terminfo database on the machine it runs on; a
  static build needs the static library, which distributions often leave out (as here), and every cross-compiled
  target needs its own copy.
- Testing: with our own driver every byte sent is ours, so tests can check it exactly (below).

The risk of our own driver is a terminal that does not follow the xterm conventions. The ones in use today do: the
Linux console, xterm, VTE terminals (GNOME Terminal and others), Konsole, kitty, Alacritty, WezTerm, foot, iTerm2,
macOS Terminal, tmux, GNU screen, and Windows Terminal (through ConPTY, for programs run under WSL or SSH). Where one
differs (the Linux console has fewer keys and no mouse), a small table chosen by `TERM` covers it; a reader of the
compiled terminfo entry can be added later if a terminal ever needs it.

## Size and resizing

- `MaxRow()` and `MaxCol()` are the model's size, which follows the device: the VT driver reads it from the terminal
  (`TIOCGWINSZ`) and learns of changes from `SIGWINCH`; the Console driver from the console's resize events; a
  window from its own.
- On a resize the model keeps the cells that still fit (from the top left), fills the new ones with blanks in the
  standard color, keeps the cursor inside, redraws the whole screen, and puts a resize event in the queue: `Inkey()`
  returns `K_RESIZE` (an xbase code, in `inkey.ch`), which an event mask may leave out. A program redraws itself from
  `MaxRow()` and `MaxCol()`, as in the example of a program that adapts to any size of at least 80 by 24.
- `SetMode( nRows, nCols )` asks the device for a size; terminals that cannot change it say so (`.F.`).

## Characters, colors and codepages

- `DispOut` and the other output functions read their strings as `SET CODEPAGE` says, so `"ação"` shows as four
  characters in UTF-8; the box strings of `box.ch` are written for the current codepage (UTF-8 box drawing
  characters by default, CP437's bytes under CP437).
- Wide characters (CJK, emoji) take two cells, as the terminal shows them; a width table generated from the Unicode
  data, as the codepage data is, tells which.
- Colors are Clipper's: `N B G BG R RB GR W`, `+` bright foreground, `*` bright background (with `SetBlink( .F. )`, as
  on VGA) or blinking, `U` underline, `I` inverse, `X` blank, and numbers; `SetColor()` returns and takes Clipper's
  color strings.

## Restoring the terminal

The VT driver puts the terminal back as it found it (mode, alternate screen, cursor, mouse reporting) when the program
ends in any way (`QUIT`, an unrecoverable error, a fatal error, a signal that ends it), and around `Ctrl+Z`
(`SIGTSTP`/`SIGCONT`) and `RUN` of an external command.

## Tests

- The Dump driver checks the model: what `@ SAY`, `Scroll`, `DispBox`, `SaveScreen`/`RestScreen`, colors and `?` leave
  on the screen, with keys from `KEYBOARD`.
- The VT driver is checked through a pseudo-terminal: a test harness (C, part of the tests) runs a program in a pty,
  types keys as a terminal sends them, changes the pty's size (which sends `SIGWINCH`), and replays the program's
  output in a small terminal emulator of its own to compare the screen it shows. The Console driver gets the same
  through ConPTY on Windows.

## Order of the work

1. The model, the Stream and Dump drivers, and the screen functions and commands (`@ SAY`, `@ CLEAR`, `CLS`,
   `@ BOX`, `@ TO`, `?` on the screen), with `SetColor` and colors; `box.ch`.
2. Events: `Inkey`, `NextKey`, `LastKey`, `KEYBOARD`, `SET KEY`, `SET TYPEAHEAD`, `inkey.ch`, event masks, resize.
3. The VT driver and its pty tests.
4. The Console driver.
5. `Alert`, `AChoice`, `MemoEdit`, `GET`/`READ`, `TBrowse`, `dbEdit`, menus, in xBase.
6. The libraries' mechanism ([architecture.md](architecture.md#libraries-written-in-xbase): `xbc --library`,
   `--lib=<name>`, the link step, `libs/`), then the Window driver and the native GUI as libraries.

## Settings without a Clipper form

What Clipper has no form for (the title of the window or terminal, whether a window may be resized, its font) is a
function of xbase's own (`WindowTitle( [cTitle] )`, `WindowResizable( [lOn] )`, `WindowFont( [cName], [nSize] )`, each
returning the setting before the call), with a `SET` command over each in `std.ch` (`SET WINDOW TITLE TO <c>`, ...).
A driver that has no such thing keeps the value and does nothing with it (a terminal's title goes through the xterm
title sequence).

## The native GUI (later)

A framework of native windows, dialogs and controls for xBase programs (event-driven, not a grid of cells) is a separate
subsystem, with its own design document before any code: one small C interface implemented natively on each platform
(Win32 on Windows, Cocoa on macOS, GTK 4 on Linux), and xBase classes and commands over it, all libraries in `libs/`. It
shares this layer's event queue model and its rule that the operating system is reached only through a backend.
