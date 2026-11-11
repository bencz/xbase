# Coverage

Inventory of the commands and runtime functions of the language, and what xbase implements. It is the work map:
update it in the same change that implements an item. Status: **done** (behavior checked against Clipper/Harbour),
*missing*.

Functions: 203 of 551 done. Commands (`std.ch` families): 62 of 157 done.

## Statements (grammar)

Statements are part of the compiler's grammar ([compiler.md](compiler.md#grammar)), not `std.ch` rules.

**FUNCTION**, **PROCEDURE**, **LOCAL**, **STATIC**, **RETURN**, **IF**, **DO CASE**, **SWITCH**, **WHILE**, **FOR**,
**FOR EACH**, **EXIT**, **LOOP**, **DO ... WITH**, **PRIVATE**, **PUBLIC**, **MEMVAR**, **FIELD**, **PARAMETERS**,
**INIT PROCEDURE**, **EXIT PROCEDURE**, **EXTERN FUNCTION** (an xbase declaration), **BEGIN SEQUENCE** (with `WITH` and
`ALWAYS`), **TRY** (an xbase statement), **WITH OBJECT**, **YIELD** (generators), **DECLARE**, **EXTERNAL** (Clipper's
name list), **ANNOUNCE**, **CLASS ... ENDCLASS** and **METHOD** (xbase's classes)

## Commands (std.ch)

Grouped by their leading words; each group covers every variant of the command.

*!*, **?**, **??**, *@*, *ACCEPT*, *APPEND*, **APPEND BLANK**, *AVERAGE*, *CALL*, **CANCEL**, *CLEAR*, *CLEAR ALL*,
*CLEAR GETS*, **CLEAR MEMORY**, *CLEAR SCREEN*, *CLEAR TYPEAHEAD*, **CLOSE**, **CLOSE ALL**, *CLOSE ALTERNATE*, **CLOSE
DATABASES**, *CLOSE FORMAT*, **CLOSE INDEXES**, *CLOSE PROCEDURE*, *CLS*, **COMMIT**, **CONTINUE**, *COPY*, *COPY FILE*,
**COUNT**, *CREATE*, **DELETE**, *DELETE FILE*, *DELETE TAG*, *DIR*, *DISPLAY*, *DO*, *EJECT*, **END**, **ENDCASE**,
**ENDDO**, **ENDFOR**, **ENDIF**, **ENDSEQUENCE**, *ERASE*, **FIND**, **GO**, **GOTO**, **INDEX ON**, *INPUT*, *JOIN*,
*KEYBOARD*, *LABEL FORM*, *LIST*, **LOCATE**, *MENU*, **NEXT**, **PACK**, **QUIT**, *READ*, *READ SAVE*, **RECALL**,
**REINDEX**, **RELEASE**, **RELEASE ALL**, *RENAME*, **REPLACE**, *REPORT FORM*, **REQUEST**, **RESTORE**, *RESTORE
SCREEN*, *RUN*, **SAVE**, **SAVE ALL**, *SAVE SCREEN*, **SAVE TO**, **SEEK**, **SELECT**, *SET ALTERNATE*, *SET
AUTOPEN*, *SET AUTORDER*, *SET AUTOSHARE*, *SET BELL*, **SET CENTURY**, **SET CODEPAGE** (an xbase command), *SET
COLOR*, *SET COLOUR*, *SET CONFIRM*, *SET CONSOLE*, *SET CURSOR*, **SET DATE**, *SET DBFLOCKSCHEME*, **SET DECIMALS**,
*SET DEFAULT*, **SET DELETED**, *SET DELIMITERS*, *SET DESCENDING*, *SET DEVICE*, *SET DIRCASE*, *SET DIRSEPARATOR*,
*SET ECHO*, *SET EOL*, **SET EPOCH**, *SET ESCAPE*, *SET EVENTMASK*, **SET EXACT**, **SET EXCLUSIVE**, *SET FILECASE*,
**SET FILTER**, *SET FIXED*, *SET FORMAT*, *SET FUNCTION*, *SET HARDCOMMIT*, *SET HEADING*, **SET INDEX**, *SET
INTENSITY*, *SET KEY*, **SET LANGUAGE** (an xbase command), *SET MARGIN*, *SET MBLOCKSIZE*, *SET MEMOBLOCK*, *SET MENU*,
*SET MESSAGE*, *SET MFILEEXT*, *SET OPTIMIZE*, **SET ORDER**, *SET PATH*, *SET PRINTER*, *SET PROCEDURE*, *SET
RELATION*, *SET SAFETY*, **SET SCOPE**, **SET SCOPEBOTTOM**, **SET SCOPETOP**, *SET SCOREBOARD*, **SET SOFTSEEK**, *SET
STATUS*, *SET STEP*, *SET STRICTREAD*, *SET TALK*, *SET TIME*, *SET TYPEAHEAD*, **SET UNIQUE**, *SET VIDEOMODE*, *SET
WRAP*, **SKIP**, *SORT*, **STORE**, **SUM**, *TEXT*, *TEXTBLOCK*, *TOTAL*, *TYPE*, **UNLOCK**, *UPDATE*, **USE**,
*WAIT*, **ZAP**

Other standard headers, each to be provided with the matching runtime support: `common.ch`, `inkey.ch`, `set.ch`
(partial), `error.ch` (provided: the severities and generic codes), `fileio.ch`, `directry.ch` and `dbstruct.ch`
(provided), `box.ch`, `color.ch`, `getexit.ch`, `memoedit.ch`, `achoice.ch`, `dbedit.ch`, `ord.ch`, `assert.ch`,
`simpleio.ch`, `blob.ch`.

## Functions

### Databases (RDD core) (71 of 112 done)

*AFields*, **Alias**, **Bof**, **dbAppend**, **dbClearFilter**, **dbClearIndex**, *dbClearRelation*, **dbCloseAll**,
**dbCloseArea**, **dbCommit**, **dbCommitAll**, **dbCreate**, **dbCreateIndex**, **dbDelete**, **dbEval**, *Dbf*,
*DBFBLOB*, *DBFDBT*, *dbFieldInfo*, *dbFileGet*, *dbFilePut*, **dbFilter**, *DBFSMT*, **dbGoBottom**, **dbGoto**,
**dbGoTop**, *dbInfo*, *dbOrderInfo*, **dbRecall**, *dbRecordInfo*, **dbReindex**, *dbRelation*, **dbRLock**,
**dbRLockList**, *dbRSelect*, **dbRUnlock**, **dbSeek**, **dbSelectArea**, *dbSetDriver*, **dbSetFilter**,
**dbSetIndex**, **dbSetOrder**, *dbSetRelation*, **dbSkip**, **dbStruct**, *dbTableExt*, **dbUnlock**, **dbUnlockAll**,
**dbUseArea**, **Deleted**, *Delim*, **Eof**, **FCount**, **FieldGet**, **FieldName**, **FieldPos**, **FieldPut**,
**FLock**, **Found**, **Header**, **IndexExt**, **IndexKey**, **IndexOrd**, **LastRec**, **Lock**, **LUpdate**,
**NetErr**, *ordBagClear*, **ordBagExt**, **ordBagName**, **ordCondSet** (its first 14 arguments), *ordCount*,
**ordCreate**, *ordCustom*, **ordDescend**, *ordDestroy*, *ordFindRec*, **ordFor**, **ordIsUnique**, **ordKey**,
*ordKeyAdd*, **ordKeyCount**, *ordKeyDel*, **ordKeyGoto**, **ordKeyNo**, *ordKeyRelPos*, **ordKeyVal**, **ordListAdd**,
**ordListClear**, **ordListRebuild**, **ordName**, **ordNumber**, **ordScope**, **ordSetFocus**, *ordSetRelation*,
*ordSkipRaw*, **ordSkipUnique**, *ordWildSeek*, *rddList*, **rddName**, *rddRegister*, **rddSetDefault**, *RDDSYS*,
**RecCount**, **RecNo**, **RecSize**, **RLock**, *SDF*, **Select**, **Used**, *_DBF*, *_dtxCondSet*

### Databases (SIx/HiPer-SEEK extensions) (0 of 115 done)

*hs_Add*, *hs_Close*, *hs_Create*, *hs_Delete*, *hs_Filter*, *hs_IfDel*, *hs_Index*, *hs_KeyCount*, *hs_Next*,
*hs_Open*, *hs_Replace*, *hs_Set*, *hs_Undelete*, *hs_Verify*, *hs_Version*, *rdd_Count*, *rdd_Info*, *rdd_Name*,
*sxChar*, *sxDate*, *sxLog*, *sxNum*, *sx_AutoOpen*, *sx_AutoShare*, *sx_Blob2File*, *sx_Chill*, *sx_ClrScope*,
*sx_Compress*, *sx_dbCreate*, *sx_DbfDecrypt*, *sx_DbfEncrypt*, *sx_Decompress*, *sx_Decrypt*, *sx_DefTrigger*,
*sx_Descend*, *sx_DToP*, *sx_Encrypt*, *sx_Error*, *sx_FCompress*, *sx_FDecompress*, *sx_File2Blob*, *sx_FileOrder*,
*sx_FNameParser*, *sx_Freeze*, *sx_GetLocks*, *sx_IDType*, *sx_IndexCount*, *sx_IndexName*, *sx_IndexType*,
*sx_IniHeader*, *sx_IsDBT*, *sx_IsFLocked*, *sx_IsLocked*, *sx_IsMyROX*, *sx_IsReadonly*, *sx_IsReindex*,
*sx_IsROXLock*, *sx_IsSem*, *sx_IsShared*, *sx_I_IndexName*, *sx_I_TagName*, *sx_KeyAdd*, *sx_KeyCount*, *sx_KeyData*,
*sx_KeyDrop*, *sx_KeyGoto*, *sx_KeyNo*, *sx_KeySincluded*, *sx_KeySkip*, *sx_KillSem*, *sx_KillTag*, *sx_LockRetry*,
*sx_MakeSem*, *sx_MemoBlk*, *sx_MemoExt*, *sx_MemoPack*, *sx_PToD*, *sx_Rlock*, *sx_Rollback*, *sx_ROXLock*,
*sx_ROXUnlock*, *sx_SeekLast*, *sx_SetFileOrd*, *sx_SetMemoBlock*, *sx_SetPass*, *sx_SetScope*, *sx_SetTag*,
*sx_SetTrigger*, *sx_SetTurbo*, *sx_SkipUnique*, *sx_SlimFast*, *sx_SortOption*, *sx_Step*, *sx_StrXCheck*,
*sx_TableName*, *sx_TableType*, *sx_TagCount*, *sx_TagInfo*, *sx_TagNo*, *sx_TagOrder*, *sx_Tags*, *sx_TagUnique*,
*sx_Thermometer*, *sx_TurboArea*, *sx_Unlock*, *sx_Version*, *sx_VFGet*, *sx_VSigLen*, *sx_Warm*, *sx_WildMatch*,
*sx_WildSeek*, *_sxOpenInit*, *_sx_IniInit*, *_sx_StrCompress*, *_sx_StrDecompress*

### Dates and times (13 of 13 done)

**CDoW**, **CMonth**, **CToD**, **Date**, **Day**, **DoW**, **DToC**, **DToS**, **Month**, **Seconds**, **SToD**,
**Time**, **Year**

### Arrays (12 of 12 done)

**AAdd**, **AClone**, **ACopy**, **ADel**, **AEval**, **AFill**, **AIns**, **Array**, **AScan**, **ASize**, **ASort**,
**ATail**

### Files, directories and environment (19 of 23 done)

**ADir**, **CurDir**, **DirChange**, **Directory**, *DiskChange*, *DiskName*, **DiskSpace**, **FClose**, **FCreate**,
**FErase**, **FError**, **File**, **FOpen**, **FRead**, **FReadStr**, **FRename**, **FSeek**, *FSetDevMod*, **FWrite**,
**GetEnv**, *IsDisk*, **MemoRead**, **MemoWrit** ([runtime.md](runtime.md#files))

### Screen, keyboard, mouse and UI (2 of 124 done)

Built on one terminal interface (screen buffer of character and color cells, cursor, keyboard) with drivers: plain
standard output (pipes, tests, servers; the default when output is not a terminal), xbase's own terminal driver (Linux,
macOS, BSD) and the Windows console ([terminal.md](terminal.md), planned). The driver is chosen when the program starts
or by a runtime switch on the command line, never by an environment variable.

*Accelerator*, *AChoice*, *Alert*, *ApplyDefau*, *Browse*, *ButtnDefCo*, *CheckBox*, *CheckDefCo*, *Col*, *ColorSelect*,
*ComboDefCo*, *dbEdit*, *DevOut*, *DevOutPict*, *DevPos*, *DispBegin*, *DispBox*, *DispCount*, *DispEnd*, *DispOut*,
*DispOutAt*, *EraseGetMsg*, *GetActive*, *GetApplyKey*, *GetClrBack*, *GetClrFore*, *GetClrPair*, *GetDoSetKey*,
*GetNew*, *GetPairLen*, *GetPairPos*, *GetPostValidate*, *GetPreValidate*, *GetReader*, *GUIApplyKey*,
*GUIPostValidate*, *GUIPreValidate*, *GUIReader*, *HitTest*, *Inkey*, *IsQuick*, *IsShortcut*, *LastKey*, *ListBDefCo*,
*ListBox*, *MaxCol*, *MaxRow*, *MCol*, *MDblClk*, *MemoEdit*, *MenuDefCol*, *MenuItem*, *MenuModal*, *MHide*,
*MLeftDown*, *MPresent*, *MRestState*, *MRightDown*, *MRow*, *MSaveState*, *MSetBounds*, *MSetCursor*, *MSetPos*,
*MShow*, *NextKey*, **OutErr**, **OutStd**, *PCol*, *Popup*, *PRow*, *PushButton*, **QOut**, **QQOut**, *RadGrDefCo*,
*RadioButton*, *RadioGroup*, *RadItDefCo*, *RangeCheck*, *ReadExit*, *ReadFormat*, *ReadInsert*, *ReadKill*,
*ReadModal*, *ReadStats*, *ReadUpdated*, *ReadVar*, *Row*, *Scroll*, *ScrollBar*, *SetBlink*, *SetClrPair*, *SetColor*,
*SetCursor*, *SetKey*, *SetPos*, *SetPosBS*, *SetPRC*, *ShowGetMsg*, *ShowMsg*, *TApplyKey*, *TBAddCol*, *TBApplyKey*,
*TBBBlock*, *TBClose*, *TBColumnNew*, *TBCreate*, *TBDelCol*, *TBDisplay*, *TBEditCell*, *TBFBlock*, *TBGoBot*,
*TBGoTop*, *TBInsCol*, *TBModal*, *TBMouse*, *TBReader*, *TBrowseNew*, *TBSBlock*, *TBSkip*, *Updated*, *_CHECKBOX_*,
*_LISTBOX_*, *_PUSHBUTT_*, *_RADIOGRP_*

### Errors (2 of 4 done)

*DosError*, **ErrorBlock**, **ErrorNew**, **ErrorSys**

### Numbers and math (20 of 21 done)

**Abs**, **Bin2I**, **Bin2L**, **Bin2W**, **Descend**, **Exp**, **I2Bin**, **Int**, *IsPrinter*, **L2Bin**, **LenNum**,
**Log**, **Max**, **Min**, **Mod**, **Round**, **Sqrt**, **Str**, **SubStr**, **Transform**, **Val**

### Strings (32 of 33 done)

**AllTrim**, **Asc**, **At**, **Chr**, **HardCR**, **IsAlpha**, **IsDigit**, **IsLower**, **IsUpper**, **Left**,
**Len**, **Lower**, **LTrim**, **MemoLine**, **MemoTran**, **MLCount**, **MLCToPos**, **MLPos**, **MPosToLC**, **PadC**,
**PadL**, **PadR**, **RAt**, **Replicate**, **Right**, **RTrim**, **SoundEx**, **Space**, **StrTran**, **StrZero**,
**Stuff**, **Upper**, *Word*

### Language, settings and runtime (19 of 94 done)

*AltD*, **AMPM**, **Break**, *cftsAdd*, *cftsClose*, *cftsCrea*, *cftsDelete*, *cftsIfDel*, *cftsNext*, *cftsOpen*,
*cftsRecN*, *cftsReplac*, *cftsSet*, *cftsUndel*, *cftsVeri*, *cftsVers*, *CLIPPER520*, *CLIPPER530*, **Days**,
*dbDrop*, *dbExists*, **DBFCDX**, *DBFFPT*, *DBFNSX*, **DBFNTX**, *DbgShadow*, *dbRename*, *DefPath*, *DirRemove*,
**Do**, **ElapTime**, **Empty**, *ErrorInHandler*, **ErrorLevel**, **Eval**, *FieldBlock*, *FieldDec*, *FieldLen*,
*FieldType*, *FieldWBlock*, *FKLabel*, *FKMax*, *Get*, *GetE*, *IOUSR_Register*, *IOUSR_SetError*, *IsAffirm*,
*IsColor*, *IsDefColor*, *IsNegative*, *MakeDir*, *Memory*, **MemVarBlock**, *MMiddleDown*, *NationMsg*, *NetName*,
*NoSnow*, *OS*, **Pad**, **PCount**, **ProcFile**, **ProcLine**, **ProcName**, **PValue**, *rddInfo*, *ReadKey*,
*RestScreen*, *SaveScreen*, **Secs**, **Set**, *SetCancel*, *SetMode*, *SetTypeahead*, *SIXCDX*, *sx_ClearOrder*,
*sx_FindRec*, *SYMBOL*, *SYSINIT*, *TBColumn*, *TBrowse*, *TBrowseDB*, *Tone*, *TopBar*, **Trim**, **TString**,
**Type**, **ValType**, *Version*, *_eInstVar*, *_GetNumCol*, *_IsGraphic*, *_NatMsgVer*, *_NatSortVer*, *_SetVideoMode*

## xbase functions

Functions of xbase's own runtime, beyond Clipper's: **ElapsedMs** (milliseconds since the program started, from a
monotonic clock, for timing), the bit functions **BitAnd**, **BitOr**, **BitXor**, **BitNor**, **BitNot**, **BitShl**,
**BitShr** (see [runtime.md](runtime.md#functions)), **Throw** (`Break()` for `TRY`), the character functions
**CharAsc**, **CharAt**, **CharChr**, **CharLeft**, **CharLen**, **CharRight**, **CharSubStr** and **CPConvert** (see
[runtime.md](runtime.md#codepages)).

Hashes ([runtime.md](runtime.md#hashes)): **Hash**, **HAllocate**, **HCaseMatch**, **HClear**, **HClone**, **HCopy**,
**HDel**, **HDelAt**, **HEval**, **HFill**, **HGet**, **HGetDef**, **HHasKey**, **HKeyAt**, **HKeys**, **HMerge**,
**HPairAt**, **HPos**, **HScan**, **HSet**, **HSort**, **HValueAt**, **HValues**.

Objects ([runtime.md](runtime.md#classes)): **ClassAssociate** (`ASSOCIATE CLASS`), **Enumerable** (the class
collections inherit; the built-in types' classes are `SCALAR CLASS`es), **ObjHasMethod**, **ObjHasVar**,
**ObjIsKindOf**, **ObjMethods**, **ObjSend**, **ObjVars**.
