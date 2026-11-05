# Coverage

Inventory of the commands and runtime functions of the language, and what xbase implements. It is the work map:
update it in the same change that implements an item. Status: **done** (behavior checked against Clipper/Harbour),
*missing*.

Functions: 55 of 603 done. Commands (`std.ch` families): 16 of 157 done.

## Statements (grammar)

Statements are part of the compiler's grammar ([compiler.md](compiler.md#grammar)), not `std.ch` rules.

**FUNCTION**, **PROCEDURE**, **LOCAL**, **STATIC**, **RETURN**, **IF**, **DO CASE**, **SWITCH**, **WHILE**,
**FOR**, **FOR EACH**, **EXIT**, **LOOP**, **DO ... WITH**, *PRIVATE*, *PUBLIC*, *MEMVAR*, *FIELD*, *PARAMETERS*,
**EXTERN FUNCTION** (an xbase declaration), *BEGIN SEQUENCE*, *DECLARE*, *EXTERNAL* (Clipper's name list), *ANNOUNCE*

## Commands (std.ch)

Grouped by their leading words; each group covers every variant of the command.

*!*, **?**, **??**, *@*, *ACCEPT*, *APPEND*, *APPEND BLANK*, *AVERAGE*, *CALL*, **CANCEL**, *CLEAR*, *CLEAR ALL*, *CLEAR
GETS*, *CLEAR MEMORY*, *CLEAR SCREEN*, *CLEAR TYPEAHEAD*, *CLOSE*, *CLOSE ALL*, *CLOSE ALTERNATE*, *CLOSE DATABASES*,
*CLOSE FORMAT*, *CLOSE INDEXES*, *CLOSE PROCEDURE*, *CLS*, *COMMIT*, *CONTINUE*, *COPY*, *COPY FILE*, *COUNT*, *CREATE*,
*DELETE*, *DELETE FILE*, *DELETE TAG*, *DIR*, *DISPLAY*, *DO*, *EJECT*, **END**, **ENDCASE**, **ENDDO**, **ENDFOR**,
**ENDIF**, *ENDSEQUENCE*, *ERASE*, *FIND*, *GO*, *GOTO*, *INDEX ON*, *INPUT*, *JOIN*, *KEYBOARD*, *LABEL FORM*, *LIST*,
*LOCATE*, *MENU*, **NEXT**, *PACK*, **QUIT**, *READ*, *READ SAVE*, *RECALL*, *REINDEX*, *RELEASE*, *RELEASE ALL*,
*RENAME*, *REPLACE*, *REPORT FORM*, *REQUEST*, *RESTORE*, *RESTORE HBV*, *RESTORE SCREEN*, *RUN*, *SAVE*, *SAVE ALL*,
*SAVE HBV*, *SAVE SCREEN*, *SAVE TO*, *SEEK*, *SELECT*, *SET ALTERNATE*, *SET AUTOPEN*, *SET AUTORDER*, *SET AUTOSHARE*,
*SET BELL*, **SET CENTURY**, *SET COLOR*, *SET COLOUR*, *SET CONFIRM*, *SET CONSOLE*, *SET CURSOR*, **SET DATE**, *SET
DBFLOCKSCHEME*, **SET DECIMALS**, *SET DEFAULT*, *SET DELETED*, *SET DELIMITERS*, *SET DESCENDING*, *SET DEVICE*, *SET
DIRCASE*, *SET DIRSEPARATOR*, *SET ECHO*, *SET EOL*, **SET EPOCH**, *SET ESCAPE*, *SET EVENTMASK*, **SET EXACT**, *SET
EXCLUSIVE*, *SET FILECASE*, *SET FILTER*, *SET FIXED*, *SET FORMAT*, *SET FUNCTION*, *SET HARDCOMMIT*, *SET HEADING*,
*SET INDEX*, *SET INTENSITY*, *SET KEY*, *SET MARGIN*, *SET MBLOCKSIZE*, *SET MEMOBLOCK*, *SET MENU*, *SET MESSAGE*,
*SET MFILEEXT*, *SET OPTIMIZE*, *SET ORDER*, *SET PATH*, *SET PRINTER*, *SET PROCEDURE*, *SET RELATION*, *SET SAFETY*,
*SET SCOPE*, *SET SCOPEBOTTOM*, *SET SCOPETOP*, *SET SCOREBOARD*, *SET SOFTSEEK*, *SET STATUS*, *SET STEP*, *SET
STRICTREAD*, *SET TALK*, *SET TIME*, *SET TYPEAHEAD*, *SET UNIQUE*, *SET VIDEOMODE*, *SET WRAP*, *SKIP*, *SORT*,
**STORE**, *SUM*, *TEXT*, *TEXTBLOCK*, *TOTAL*, *TYPE*, *UNLOCK*, *UPDATE*, *USE*, *WAIT*, *ZAP*

Other standard headers, each to be provided with the matching runtime support: `common.ch`, `inkey.ch`,
`set.ch` (partial), `error.ch`, `fileio.ch`, `box.ch`, `color.ch`, `dbstruct.ch`, `directry.ch`, `getexit.ch`,
`memoedit.ch`, `achoice.ch`, `dbedit.ch`, `ord.ch`, `assert.ch`, `simpleio.ch`, `blob.ch`, `hbclass.ch`
(classes are native in xbase), `hbsix.ch`.

## Functions

### Databases (RDD core) (0 of 112 done)

*AFields*, *Alias*, *Bof*, *dbAppend*, *dbClearFilter*, *dbClearIndex*, *dbClearRelation*, *dbCloseAll*, *dbCloseArea*,
*dbCommit*, *dbCommitAll*, *dbCreate*, *dbCreateIndex*, *dbDelete*, *dbEval*, *Dbf*, *DBFBLOB*, *DBFDBT*,
*dbFieldInfo*, *dbFileGet*, *dbFilePut*, *dbFilter*, *DBFSMT*, *dbGoBottom*, *dbGoto*, *dbGoTop*, *dbInfo*,
*dbOrderInfo*, *dbRecall*, *dbRecordInfo*, *dbReindex*, *dbRelation*, *dbRLock*, *dbRLockList*, *dbRSelect*,
*dbRUnlock*, *dbSeek*, *dbSelectArea*, *dbSetDriver*, *dbSetFilter*, *dbSetIndex*, *dbSetOrder*, *dbSetRelation*,
*dbSkip*, *dbStruct*, *dbTableExt*, *dbUnlock*, *dbUnlockAll*, *dbUseArea*, *Deleted*, *Delim*, *Eof*, *FCount*,
*FieldGet*, *FieldName*, *FieldPos*, *FieldPut*, *FLock*, *Found*, *Header*, *IndexExt*, *IndexKey*, *IndexOrd*,
*LastRec*, *Lock*, *LUpdate*, *NetErr*, *ordBagClear*, *ordBagExt*, *ordBagName*, *ordCondSet*, *ordCount*,
*ordCreate*, *ordCustom*, *ordDescend*, *ordDestroy*, *ordFindRec*, *ordFor*, *ordIsUnique*, *ordKey*, *ordKeyAdd*,
*ordKeyCount*, *ordKeyDel*, *ordKeyGoto*, *ordKeyNo*, *ordKeyRelPos*, *ordKeyVal*, *ordListAdd*, *ordListClear*,
*ordListRebuild*, *ordName*, *ordNumber*, *ordScope*, *ordSetFocus*, *ordSetRelation*, *ordSkipRaw*, *ordSkipUnique*,
*ordWildSeek*, *rddList*, *rddName*, *rddRegister*, *rddSetDefault*, *RDDSYS*, *RecCount*, *RecNo*, *RecSize*, *RLock*,
*SDF*, *Select*, *Used*, *_DBF*, *_dtxCondSet*

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

### Dates and times (11 of 13 done)

**CDoW**, **CMonth**, **CToD**, **Date**, **Day**, **DoW**, **DToC**, **DToS**, **Month**, *Seconds*, **SToD**, *Time*,
**Year**

### Arrays (5 of 12 done)

**AAdd**, **AClone**, *ACopy*, *ADel*, *AEval*, *AFill*, *AIns*, **Array**, *AScan*, **ASize**, *ASort*, **ATail**

### Files, directories and environment (0 of 23 done)

*ADir*, *CurDir*, *DirChange*, *Directory*, *DiskChange*, *DiskName*, *DiskSpace*, *FClose*, *FCreate*, *FErase*,
*FError*, *File*, *FOpen*, *FRead*, *FReadStr*, *FRename*, *FSeek*, *FSetDevMod*, *FWrite*, *GetEnv*, *IsDisk*,
*MemoRead*, *MemoWrit*

### Screen, keyboard, mouse and UI (2 of 126 done)

Built on one terminal interface (screen buffer of character and color cells, cursor, keyboard) with
drivers: plain standard output (pipes, tests, servers; the default when output is not a terminal),
ncurses (Linux, macOS, BSD) and the Windows console. The driver is chosen by a command-line option or
at link time, never by an environment variable.

*Accelerator*, *AChoice*, *Alert*, *ApplyDefau*, *Browse*, *ButtnDefCo*, *CheckBox*, *CheckDefCo*, *Col*,
*ColorSelect*, *ComboDefCo*, *dbEdit*, *DevOut*, *DevOutPict*, *DevPos*, *DispBegin*, *DispBox*, *DispCount*,
*DispEnd*, *DispOut*, *DispOutAt*, *EraseGetMsg*, *GetActive*, *GetApplyKey*, *GetClrBack*, *GetClrFore*, *GetClrPair*,
*GetDoSetKey*, *GetNew*, *GetPairLen*, *GetPairPos*, *GetPostValidate*, *GetPreValidate*, *GetReader*, *GUIApplyKey*,
*GUIPostValidate*, *GUIPreValidate*, *GUIReader*, *HBClass*, *HitTest*, *Inkey*, *IsQuick*, *IsShortcut*, *LastKey*,
*ListBDefCo*, *ListBox*, *MaxCol*, *MaxRow*, *MCol*, *MDblClk*, *MemoEdit*, *MenuDefCol*, *MenuItem*, *MenuModal*,
*MHide*, *MLeftDown*, *MPresent*, *MRestState*, *MRightDown*, *MRow*, *MSaveState*, *MSetBounds*, *MSetCursor*,
*MSetPos*, *MShow*, *NextKey*, *OutErr*, *OutStd*, *PCol*, *Popup*, *PRow*, *PushButton*, **QOut**, **QQOut**,
*RadGrDefCo*, *RadioButto*, *RadioButton*, *RadioGroup*, *RadItDefCo*, *RangeCheck*, *ReadExit*, *ReadFormat*,
*ReadInsert*, *ReadKill*, *ReadModal*, *ReadStats*, *ReadUpdated*, *ReadVar*, *Row*, *Scroll*, *ScrollBar*, *SetBlink*,
*SetClrPair*, *SetColor*, *SetCursor*, *SetKey*, *SetPos*, *SetPosBS*, *SetPRC*, *ShowGetMsg*, *ShowMsg*, *TApplyKey*,
*TBAddCol*, *TBApplyKey*, *TBBBlock*, *TBClose*, *TBColumnNew*, *TBCreate*, *TBDelCol*, *TBDisplay*, *TBEditCell*,
*TBFBlock*, *TBGoBot*, *TBGoTop*, *TBInsCol*, *TBModal*, *TBMouse*, *TBReader*, *TBrowseNew*, *TBSBlock*, *TBSkip*,
*Updated*, *_CHECKBOX_*, *_LISTBOX_*, *_PUSHBUTT_*, *_RADIOGRP_*

### Errors (0 of 4 done)

*DosError*, *ErrorBlock*, *ErrorNew*, *ErrorSys*

### Numbers and math (11 of 21 done)

**Abs**, *Bin2I*, *Bin2L*, *Bin2W*, *Descend*, **Exp**, *I2Bin*, **Int**, *IsPrinter*, *L2Bin*, *LenNum*, **Log**,
**Max**, **Min**, *Mod*, **Round**, **Sqrt**, **Str**, **SubStr**, *Transform*, **Val**

### Strings (21 of 33 done)

**AllTrim**, **Asc**, **At**, **Chr**, *HardCR*, **IsAlpha**, **IsDigit**, **IsLower**, **IsUpper**, **Left**, **Len**,
**Lower**, **LTrim**, *MemoLine*, *MemoTran*, *MLCount*, *MLCToPos*, *MLPos*, *MPosToLC*, *PadC*, *PadL*, *PadR*,
**RAt**, **Replicate**, **Right**, **RTrim**, *SoundEx*, **Space**, **StrTran**, **StrZero**, **Stuff**, **Upper**,
*Word*

### Language, settings and runtime (5 of 144 done)

*AltD*, *AMPM*, *Break*, *cftsAdd*, *cftsClose*, *cftsCrea*, *cftsDelete*, *cftsIfDel*, *cftsNext*, *cftsOpen*,
*cftsRecN*, *cftsReplac*, *cftsSet*, *cftsUndel*, *cftsVeri*, *cftsVers*, *CLIPPER520*, *CLIPPER530*, *Days*,
*dbClearFil*, *dbClearRel*, *dbCloseAre*, *dbDrop*, *dbExists*, *DBFCDX*, *DBFFPT*, *dbFieldInf*, *DBFNSX*, *DBFNTX*,
*DbgShadow*, *dbOrderInf*, *dbRecordIn*, *dbRename*, *dbRLockLis*, *dbSelectAr*, *dbSetDrive*, *dbSetFilte*,
*dbSetRelat*, *DefPath*, *DirRemove*, *Do*, *ElapTime*, **Empty**, *ErrorInHan*, *ErrorInHandler*, *ErrorLevel*,
**Eval**, *FieldBlock*, *FieldDec*, *FieldLen*, *FieldType*, *FieldWBlock*, *FKLabel*, *FKMax*, *Get*, *GetE*,
*HBCheckBox*, *HBEditor*, *HBGetList*, *HBHash*, *HBLabelForm*, *HBListBox*, *HBMemoEditor*, *HBMenuItem*, *HBMenuSys*,
*HBObject*, *HBPersistent*, *HBPointer*, *HBPopupMenu*, *HBProfile*, *HBProfileEntity*, *HBProfileFunction*,
*HBProfileLowLevel*, *HBProfileMethod*, *HBProfileOpcode*, *HBProfileReport*, *HBProfileReportToArray*,
*HBProfileReportToFile*, *HBProfileReportToString*, *HBProfileReportToTBrowse*, *HBPushButton*, *HBRadioButton*,
*HBRadioGroup*, *HBReportForm*, *HBScalar*, *HBScrollBar*, *HBTextLine*, *HBTopBarMenu*, *IOUSR_Register*,
*IOUSR_SetError*, *IsAffirm*, *IsColor*, *IsDefColor*, *IsNegative*, *MakeDir*, *Memory*, *MemVarBlock*, *MMiddleDown*,
*NationMsg*, *NetName*, *NoSnow*, *ordListCle*, *ordListReb*, *ordSetFocu*, *ordSetRela*, *OS*, *Pad*, *PCount*,
*ProcFile*, *ProcLine*, *ProcName*, *PValue*, *rddInfo*, *rddSetDefa*, *ReadKey*, *RestScreen*, *SaveScreen*, *Secs*,
**Set**, *SetCancel*, *SetMode*, *SetTypeahead*, *SIXCDX*, *sx_ClearOrder*, *sx_FindRec*, *sx_SetTrig*, *SYMBOL*,
*SYSINIT*, *TBColumn*, *TBrowse*, *TBrowseDB*, *Tone*, *TopBar*, **Trim**, *TString*, *Type*, **ValType**, *Version*,
*_eInstVar*, *_GetNumCol*, *_IsGraphic*, *_NatMsgVer*, *_NatSortVer*, *_SetVideoMode*

## Harbour extensions

Programs written for Harbour also use its `hb_*` functions (about 700: hashes, strings, files, threads, ...).
They are added by area together with the matching Clipper-level support; hashes come with the hash type.
