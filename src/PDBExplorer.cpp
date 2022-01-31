#include "PDBExplorer.h"

PDBExplorer::PDBExplorer(QWidget* parent) : QMainWindow(parent)
{
    ui.setupUi(this);

    setWindowTitle("PDB Explorer");
    setAcceptDrops(true);
    removeToolBar(ui.mainToolBar);

    ui.btnSearchSymbol->setVisible(false);
    ui.btnSearchSymbol->setAutoDefault(true);

    menu = new QMenu(this);
    action = new QAction("Export", this);
    symbolsViewProxyModel = new QSortFilterProxyModel(ui.tvSymbols);
    peHeaderParser = new PEHeaderParser();
    pdb = new PDB(nullptr, &options, peHeaderParser, &diaSymbols, &symbolRecords, &variables, &functions, &publicSymbols);

    ui.tvSymbols->setModel(symbolsViewProxyModel);
    ui.tvSymbols->setContextMenuPolicy(Qt::CustomContextMenu);

    menu->addAction(action);

    OptionsDialog::LoadOptions(&options);

    connect(ui.txtSearchSymbol, &QLineEdit::returnPressed, this, &PDBExplorer::BtnSearchSymbolClicked);
    connect(ui.tvSymbols, &QTableView::customContextMenuRequested, this, &PDBExplorer::CustomMenuRequested);
    connect(action, &QAction::triggered, this, &PDBExplorer::ExportSymbol);
    connect(peHeaderParser, &PEHeaderParser::SendStatusMessage, this, &PDBExplorer::DisplayStatusMessage);
    connect(pdb, &PDB::SendStatusMessage, this, &PDBExplorer::DisplayStatusMessage);

    connect(ui.actionOpen, &QAction::triggered, this, &PDBExplorer::OpenActionTriggered);
    connect(ui.actionExit, &QAction::triggered, this, &PDBExplorer::ExitActionTriggered);
    connect(ui.actionOptions, &QAction::triggered, this, &PDBExplorer::OptionsActionTriggered);
    connect(ui.actionExportAllTypes, &QAction::triggered, this, &PDBExplorer::ExportAllTypesActionTriggered);
    connect(ui.actionClearCache, &QAction::triggered, this, &PDBExplorer::ClearCacheActionTriggered);

    connect(ui.txtSearchSymbol, &QLineEdit::textChanged, this, &PDBExplorer::TxtSearchSymbolTextChanged);
    connect(ui.btnSearchSymbol, &QPushButton::clicked, this, &PDBExplorer::BtnSearchSymbolClicked);
    connect(ui.tvSymbols, &QTableView::clicked, this, &PDBExplorer::TVSymbolsClicked);
    
    connect(ui.chkClasses, &QCheckBox::toggled, this, &PDBExplorer::ChkClassesToggled);
    connect(ui.chkStructs, &QCheckBox::toggled, this, &PDBExplorer::ChkStructsToggled);
    connect(ui.chkUnions, &QCheckBox::toggled, this, &PDBExplorer::ChkUnionsToggled);
    connect(ui.chkEnums, &QCheckBox::toggled, this, &PDBExplorer::ChkEnumsToggled);
    connect(ui.chkGlobal, &QCheckBox::toggled, this, &PDBExplorer::ChkGlobalToggled);
    connect(ui.chkStatic, &QCheckBox::toggled, this, &PDBExplorer::ChkStaticToggled);
    connect(ui.chkMember, &QCheckBox::toggled, this, &PDBExplorer::ChkMemberToggled);

    ui.tabWidget->removeTab(1);
    ui.tabWidget->removeTab(1);

    SetupLayouts();

    //exportSymbolNames2();
    //checkToWhichPDBFilesBelongSymbolNames();
    //exportSymbolsFromMutltiplePDBFiles();
    //exportCPPFiles();
    //exportEnums();

    QStringList symbolTypes;
    QStringList displayOptions;

    symbolTypes.append("UDTs and enums");
    symbolTypes.append("Data");
    symbolTypes.append("Functions");
    symbolTypes.append("Public symbols");

    displayOptions.append("Header code");
    displayOptions.append("Source code");
    displayOptions.append("Structure view");
    displayOptions.append("Function offsets");
    displayOptions.append("VTables");
    displayOptions.append("Flattened UDT layout");
    displayOptions.append("VTables layout");
    displayOptions.append("MSVC style layout");
    displayOptions.append("Modules");
    displayOptions.append("Lines");
    displayOptions.append("Imports");
    displayOptions.append("Exports");
    displayOptions.append("Strings");
    displayOptions.append("Assembly");
    displayOptions.append("Assembly for single function");
    displayOptions.append("MSVC demangler");
    displayOptions.append("Address converter");
    displayOptions.append("IDA pseudo code");

    ui.cbSymbolTypes->addItems(symbolTypes);
    ui.cbDisplayOptions->addItems(displayOptions);
    ui.cbDisplayOptions->setEnabled(false);

    isFileOpened = false;

    msvcDemangler = MSVCDemangler();
}

void PDBExplorer::SetupCodeEdtor(ScintillaEdit* codeEditor)
{
    const char* cppKeywords = "alignof and and_eq bitand bitor break case catch compl const_cast continue default delete "
        "do dynamic_cast else false for goto if namespace new not not_eq nullptr operator or or_eq reinterpret_cast return "
        "sizeof static_assert static_cast switch this throw true try typedef typeid using while xor xor_eq NULL alignas asm "
        "auto bool char char16_t char32_t class clock_t const constexpr decltype double enum explicit export extern final "
        "float friend inline int int8_t int16_t int32_t int64_t int_fast8_t int_fast16_t int_fast32_t int_fast64_t intmax_t "
        "intptr_t long mutable noexcept override private protected ptrdiff_t public register short signed size_t ssize_t "
        "static struct template thread_local time_t typename uint8_t uint16_t uint32_t uint64_t uint_fast8_t uint_fast16_t "
        "uint_fast32_t uint_fast64_t uintmax_t uintptr_t union unsigned virtual void volatile wchar_t __int16 __int32 __int64 "
        "__int8 __declspec novtable finally __asm __asume __based __box __cdecl __delegate delegate deprecated dllexport "
        "dllimport event __event __except __fastcall __finally __forceinline __int128 __interface interface __leave naked "
        "noinline __noop noreturn nothrow safecast __stdcall __try __unaligned uuid __uuidof __virtual_inheritance empty_bases "
        "__thiscall __vectorcall __clrcall";

    QFunctionPointer fn = QLibrary::resolve("lexilla", "CreateLexer");
    void* cppLexer = (reinterpret_cast<Lexilla::CreateLexerFn>(fn))("cpp");

    codeEditor->setILexer(reinterpret_cast<sptr_t>(cppLexer));
    codeEditor->styleSetFont(STYLE_DEFAULT, "Consolas");
    codeEditor->styleSetSize(STYLE_DEFAULT, 14);
    codeEditor->styleClearAll();
    codeEditor->setTabWidth(4);

    codeEditor->setKeyWords(0, cppKeywords);
    codeEditor->styleSetFore(SCE_C_WORD, 0x00FF0000);
    codeEditor->styleSetFore(SCE_C_STRING, 0x001515A3);
    codeEditor->styleSetFore(SCE_C_CHARACTER, 0x001515A3);
    codeEditor->styleSetFore(SCE_C_PREPROCESSOR, 0x00808080);
    codeEditor->styleSetFore(SCE_C_COMMENT, 0x00008000);
    codeEditor->styleSetFore(SCE_C_COMMENTLINE, 0x00008000);
    codeEditor->styleSetFore(SCE_C_COMMENTDOC, 0x00008000);
    codeEditor->styleSetFore(SCE_C_NUMBER, 0x00800000);
    codeEditor->setCaretLineVisible(TRUE);
    codeEditor->setCaretLineBack(0xB0FFFF);
}

void PDBExplorer::SetupAssemblyEditor(ScintillaEdit* assemblyEditor)
{
    const char* assemblyKeywordSet1 = "aaa aad aam aas adc add and call cbw cdqe clc cld cli cmc cmp cmps cmpsb cmpsw cwd daa das "
        "dec div esc hlt idiv imul in inc int into iret ja jae jb jbe jc jcxz je jg jge jl jle jmp jna jnae jnb jnbe jnc jne "
        "jng jnge jnl jnle jno jnp jns jnz jo jp jpe jpo js jz lahf lds lea les lods lodsb lodsw loop loope loopew loopne "
        "loopnew loopnz loopnzw loopw loopz loopzw mov movabs movs movsb movsw mul neg nop not or out pop popf push pushf rcl "
        "rcr ret retf retn rol ror sahf sal sar sbb scas scasb scasw shl shr stc std sti stos stosb stosw sub test wait xchg "
        "xlat xlatb xor bound enter ins insb insw leave outs outsb outsw popa pusha pushw arpl lar lsl sgdt sidt sldt smsw str "
        "verr verw clts lgdt lidt lldt lmsw ltr bsf bsr bt btc btr bts cdq cmpsd cwde insd iretd iretdf iretf jecxz lfs lgs "
        "lodsd loopd looped loopned loopnzd loopzd lss movsd movsx movsxd movzx outsd popad popfd pushad pushd pushfd scasd "
        "seta setae setb setbe setc sete setg setge setl setle setna setnae setnb setnbe setnc setne setng setnge setnl setnle "
        "setno setnp setns setnz seto setp setpe setpo sets setz shld shrd stosd bswap cmpxchg invd invlpg wbinvd xadd lock "
        "rep repe repne repnz repz cflush cpuid emms femms cmovo cmovno cmovb cmovc cmovnae cmovae cmovnb cmovnc cmove cmovz "
        "cmovne cmovnz cmovbe cmovna cmova cmovnbe cmovs cmovns cmovp cmovpe cmovnp cmovpo cmovl cmovnge cmovge cmovnl cmovle "
        "cmovng cmovg cmovnle cmpxchg486 cmpxchg8b loadall loadall286 ibts icebp int1 int3 int01 int03 iretw popaw popfw pushaw "
        "pushfw rdmsr rdpmc rdshr rdtsc rsdc rsldt rsm rsts salc smi smint smintold svdc svldt svts syscall sysenter sysexit "
        "sysret ud0 ud1 ud2 umov xbts wrmsr wrshr";

    const char* assemblyKeywordSet2 = "f2xm1 fabs fadd faddp fbld fbstp fchs fclex fcom fcomp fcompp fdecstp fdisi fdiv fdivp "
        "fdivr fdivrp feni ffree fiadd "
        "ficom ficomp fidiv fidivr fild fimul fincstp finit fist fistp fisub fisubr fld fld1 fldcw fldenv fldenvw fldl2e fldl2t "
        "fldlg2 fldln2 fldpi fldz fmul fmulp fnclex fndisi fneni fninit fnop fnsave fnsavew fnstcw fnstenv fnstenvw fnstsw "
        "fpatan fprem fptan frndint frstor frstorw fsave fsavew fscale fsqrt fst fstcw fstenv fstenvw fstp fstsw fsub fsubp "
        "fsubr fsubrp ftst fwait fxam fxch fxtract fyl2x fyl2xp1 fsetpm fcos fldenvd fnsaved fnstenvd fprem1 frstord fsaved fsin "
        "fsincos fstenvd fucom fucomp fucompp fcomi fcomip ffreep fcmovb fcmove fcmovbe fcmovu fcmovnb fcmovne fcmovnbe fcmovnu";

    const char* assemblyKeywordSet3 = "ah al ax bh bl bp bx ch cl cr0 cr2 cr3 cr4 cs cx dh di dl dr0 dr1 dr2 dr3 dr6 dr7 "
        "ds dx eax ebp ebx ecx edi edx "
        "es esi esp fs gs rax rbx rcx rdx rdi rsi rbp rsp r8 r9 r10 r11 r12 r13 r14 r15 r8d r9d r10d r11d r12d r13d r14d r15d "
        "r8w r9w r10w r11w r12w r13w r14w r15w r8b r9b r10b r11b r12b r13b r14b r15b si sp ss st tr3 tr4 tr5 tr6 tr7 "
        "st0 st1 st2 st3 st4 st5 st6 st7 mm0 mm1 mm2 mm3 mm4 mm5 mm6 mm7 xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 "
        "xmm10 xmm11 xmm12 xmm13 xmm14 xmm15";

    const char* assemblyKeywordSet4 = ".186 .286 .286c .286p .287 .386 .386c .386p .387 .486 .486p .8086 "
        ".8087.alpha .break.code .const .continue.cref.data.data "
        "? .dosseg .else.elseif.endif.endw.err.err1.err2.errb.errdef.errdif.errdifi.erre.erridn.erridni.errnb.errndef.errnz.exit.fardata.fardata "
        "? .if.lall.lfcond.list.listall.listif.listmacro.listmacroall.model.no87.nocref.nolist.nolistif.nolistmacro.radix.repeat.sall.seq.sfcond.stack.startup.tfcond.type.until.untilcxz "
        ".while.xall.xcref.xlist alias align assume catstr comm comment db dd df dosseg dq dt dup dw echo else elseif "
        "elseif1 elseif2 elseifb elseifdef elseifdif elseifdifi elseife elseifidn elseifidni elseifnb elseifndef end endif "
        "endm endp ends eq equ even exitm extern externdef extrn for forc ge goto group gt high highword if if1 if2 ifb ifdef "
        "ifdif ifdifi ife ifidn ifidni ifnb ifndef include includelib instr invoke irp irpc label le length lengthof local "
        "low lowword lroffset lt macro mask mod.msfloat name ne offset opattr option org% out page popcontext proc proto ptr "
        "public purge pushcontext record repeat rept seg segment short size sizeof sizestr struc struct substr subtitle subttl "
        "textequ this title type typedef union while width resb resw resd resq rest incbin times% define% idefine% xdefine% "
        "xidefine% undef% assign% iassign% strlen% substr% macro% imacro% endmacro% rotate% if% elif% else% endif% ifdef% "
        "ifndef% elifdef% elifndef% ifmacro% ifnmacro% elifmacro% elifnmacro% ifctk% ifnctk% elifctk% elifnctk% ifidn% ifnidn% "
        "elifidn% elifnidn% ifidni% ifnidni% elifidni% elifnidni% ifid% ifnid% elifid% elifnid% ifstr% ifnstr% elifstr% "
        "elifnstr% ifnum% ifnnum% elifnum% elifnnum% error% rep% endrep% exitrep% include% push% pop% repl endstruc istruc at "
        "iend alignb% arg% stacksize% local% line bits use16 use32 section absolute global common cpu import export";

    const char* assemblyKeywordSet5 = "$ ? @b @f addr basic byte c carry ? dword far far16 fortran fword near near16 overflow "
        "? parity ? pascal qword "
        "real4 real8 real10 sbyte sdword sign ? stdcall sword syscall tbyte vararg word zero ? flat near32 far32 abs all "
        "assumes at casemap common compact cpu dotname emulator epilogue error export expr16 expr32 farstack forceframe "
        "huge language large listing ljmp loadds m510 medium memory nearstack nodotname noemulator nokeyword noljmp nom510 "
        "none nonunique nooldmacros nooldstructs noreadonly noscoped nosignextend nothing notpublic oldmacros oldstructs "
        "os_dos para private prologue radix readonly req scoped setif2 smallstack tiny use16 use32 uses a16 a32 o16 o32 "
        "nosplit $$ seq wrt small.text.data.bss % 0 % 1 % 2 % 3 % 4 % 5 % 6 % 7 % 8 % 9";

    const char* assemblyKeywordSet6 = "addpd addps addsd addss andpd andps andnpd andnps cmpeqpd cmpltpd cmplepd "
        "cmpunordpd cmpnepd cmpnltpd cmpnlepd "
        "cmpordpd cmpeqps cmpltps cmpleps cmpunordps cmpneps cmpnltps cmpnleps cmpordps cmpeqsd cmpltsd cmplesd cmpunordsd "
        "cmpnesd cmpnltsd cmpnlesd cmpordsd cmpeqss cmpltss cmpless cmpunordss cmpness cmpnltss cmpnless cmpordss comisd "
        "comiss cvtdq2pd cvtdq2ps cvtpd2dq cvtpd2pi cvtpd2ps cvtpi2pd cvtpi2ps cvtps2dq cvtps2pd cvtps2pi cvtss2sd cvtss2si "
        "cvtsd2si cvtsd2ss cvtsi2sd cvtsi2ss cvttpd2dq cvttpd2pi cvttps2dq cvttps2pi cvttsd2si cvttss2si divpd divps divsd "
        "divss fxrstor fxsave ldmxscr lfence mfence maskmovdqu maskmovdq maxpd maxps paxsd maxss minpd minps minsd minss "
        "movapd movaps movdq2q movdqa movdqu movhlps movhpd movhps movd movq movlhps movlpd movlps movmskpd movmskps movntdq "
        "movnti movntpd movntps movntq movq2dq movsd movss movupd movups mulpd mulps mulsd mulss orpd orps packssdw packsswb "
        "packuswb paddb paddsb paddw paddsw paddd paddsiw paddq paddusb paddusw pand pandn pause paveb pavgb pavgw pavgusb "
        "pdistib pextrw pcmpeqb pcmpeqw pcmpeqd pcmpgtb pcmpgtw pcmpgtd pf2id pf2iw pfacc pfadd pfcmpeq pfcmpge pfcmpgt pfmax "
        "pfmin pfmul pmachriw pmaddwd pmagw pmaxsw pmaxub pminsw pminub pmovmskb pmulhrwc pmulhriw pmulhrwa pmulhuw pmulhw "
        "pmullw pmuludq pmvzb pmvnzb pmvlzb pmvgezb pfnacc pfpnacc por prefetch prefetchw prefetchnta prefetcht0 prefetcht1 "
        "prefetcht2 pfrcp pfrcpit1 pfrcpit2 pfrsqit1 pfrsqrt pfsub pfsubr pi2fd pinsrw psadbw pshufd pshufhw pshuflw pshufw "
        "psllw pslld psllq pslldq psraw psrad psrlw psrld psrlq psrldq psubb psubw psubd psubq psubsb psubsw psubusb psubusw "
        "psubsiw pswapd punpckhbw punpckhwd punpckhdq punpckhqdq punpcklbw punpcklwd punpckldq punpcklqdq pxor rcpps rcpss "
        "rsqrtps rsqrtss sfence shufpd shufps sqrtpd sqrtps sqrtsd sqrtss stmxcsr subpd subps subsd subss ucomisd ucomiss "
        "unpckhpd unpckhps unpcklpd unpcklps xorpd xorps";

    QFunctionPointer fn = QLibrary::resolve("lexilla", "CreateLexer");
    void* asmLexer = (reinterpret_cast<Lexilla::CreateLexerFn>(fn))("asm");

    assemblyEditor->setILexer(reinterpret_cast<sptr_t>(asmLexer));
    assemblyEditor->styleSetFont(STYLE_DEFAULT, "Consolas");
    assemblyEditor->styleSetSize(STYLE_DEFAULT, 14);
    assemblyEditor->styleClearAll();
    assemblyEditor->setTabWidth(4);

    assemblyEditor->setKeyWords(0, assemblyKeywordSet1);
    assemblyEditor->setKeyWords(1, assemblyKeywordSet2);
    assemblyEditor->setKeyWords(2, assemblyKeywordSet3);
    assemblyEditor->setKeyWords(3, assemblyKeywordSet4);
    assemblyEditor->setKeyWords(4, assemblyKeywordSet5);
    assemblyEditor->setKeyWords(5, assemblyKeywordSet6);
    assemblyEditor->styleSetFore(SCE_ASM_COMMENT, 0x008000);
    assemblyEditor->styleSetFore(SCE_ASM_NUMBER, 0x0080FF);
    assemblyEditor->styleSetFore(SCE_ASM_STRING, 0x808080);
    assemblyEditor->styleSetFore(SCE_ASM_OPERATOR, 0x800000);
    assemblyEditor->styleSetFore(SCE_ASM_IDENTIFIER, 0x000000);
    assemblyEditor->styleSetFore(SCE_ASM_CPUINSTRUCTION, 0xFF0000);
    assemblyEditor->styleSetFore(SCE_ASM_MATHINSTRUCTION, 0xC08000);
    assemblyEditor->styleSetFore(SCE_ASM_REGISTER, 0xFF8080);
    assemblyEditor->styleSetBack(SCE_ASM_REGISTER, 0xCCFFFF);
    assemblyEditor->styleSetFore(SCE_ASM_DIRECTIVE, 0xFF8000);
    assemblyEditor->styleSetFore(SCE_ASM_DIRECTIVEOPERAND, 0x800000);
    assemblyEditor->styleSetFore(SCE_ASM_COMMENTBLOCK, 0x008000);
    assemblyEditor->styleSetFore(SCE_ASM_CHARACTER, 0x008080);
    assemblyEditor->styleSetFore(SCE_ASM_EXTINSTRUCTION, 0x004080);
    assemblyEditor->setCaretLineVisible(TRUE);
    assemblyEditor->setCaretLineBack(0xB0FFFF);
}

void PDBExplorer::SetupLayouts()
{
    codeEditor = new ScintillaEdit(this);
    assemblyEditor = new ScintillaEdit(this);
    assemblyEditor2 = new ScintillaEdit(this);
    assemblyEditor3 = new ScintillaEdit(this);
    pseudoCodeEditor = new ScintillaEdit(this);
    pseudoCodeEditor2 = new ScintillaEdit(this);

    SetupCodeEdtor(codeEditor);
    SetupAssemblyEditor(assemblyEditor);
    SetupAssemblyEditor(assemblyEditor2);
    SetupAssemblyEditor(assemblyEditor3);
    SetupCodeEdtor(pseudoCodeEditor);
    SetupCodeEdtor(pseudoCodeEditor2);

    QHBoxLayout* layout1 = new QHBoxLayout(this);
    QBoxLayout* layout2 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QBoxLayout* layout3 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QHBoxLayout* layout4 = new QHBoxLayout(this);
    QHBoxLayout* layout5 = new QHBoxLayout(this);
    QBoxLayout* layout6 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QHBoxLayout* layout7 = new QHBoxLayout(this);
    QBoxLayout* layout8 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QBoxLayout* layout9 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QBoxLayout* layout10 = new QBoxLayout(QBoxLayout::TopToBottom, this);
    QHBoxLayout* layout11 = new QHBoxLayout(this);

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel2 = new QSortFilterProxyModel(this);
    model = new QStringListModel(this);
    txtFindItem = new QLineEdit(this);
    txtMangledName = new QLineEdit(this);
    txtDemangledName = new QLineEdit(this);
    txtMangledName2 = new QLineEdit(this);
    txtDemangledName2 = new QLineEdit(this);
    txtAddress = new QLineEdit(this);
    txtAddress2 = new QLineEdit(this);
    txtVirtualAddress = new QLineEdit(this);
    txtRelativeVirtualAddress = new QLineEdit(this);
    txtFileOffset = new QLineEdit(this);
    plainTextEdit = new QPlainTextEdit(this);
    tableView = new QTableView(this);
    tvVTables = new QTableView(this);
    cbAddressTypes = new QComboBox(this);
    cbAddressTypes2 = new QComboBox(this);

    QHBoxLayout* horizontalLayout = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout2 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout3 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout4 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout5 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout6 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout7 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout8 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout9 = new QHBoxLayout(this);
    QHBoxLayout* horizontalLayout10 = new QHBoxLayout(this);
    QLabel* lblFindItem = new QLabel(this);
    QLabel* lblMangledName = new QLabel(this);
    QLabel* lblDemangledName = new QLabel(this);
    QLabel* lblMangledName2 = new QLabel(this);
    QLabel* lblDemangledName2 = new QLabel(this);
    QLabel* lblAddress = new QLabel(this);
    QLabel* lblAddress2 = new QLabel(this);
    QLabel* lblVirtualAddress = new QLabel(this);
    QLabel* lblRelativeVirtualAddress = new QLabel(this);
    QLabel* lblFileOffset = new QLabel(this);
    QListView* lvVTables = new QListView(this);

    connect(txtFindItem, &QLineEdit::textChanged, this, &PDBExplorer::TxtFindItemTextChanged);
    connect(lvVTables, &QListView::clicked, this, &PDBExplorer::LVVTablesClicked);
    connect(txtMangledName, &QLineEdit::textChanged, this, &PDBExplorer::TxtMangledNameTextChanged);
    connect(txtMangledName2, &QLineEdit::textChanged, this, &PDBExplorer::TxtMangledName2TextChanged);
    connect(txtVirtualAddress, &QLineEdit::textChanged, this, &PDBExplorer::TxtVirtualAddressTextChanged);
    connect(txtRelativeVirtualAddress, &QLineEdit::textChanged, this, &PDBExplorer::TxtRelativeVirtualAddressTextChanged);
    connect(txtFileOffset, &QLineEdit::textChanged, this, &PDBExplorer::TxtFileOffsetTextChanged);

    layout1->addWidget(codeEditor);
    layout1->setMargin(0);
    layout1->setSpacing(0);

    lblMangledName->setText("Mangled name:");
    lblDemangledName->setText("Demangled name:");
    txtMangledName->setMaximumHeight(30);
    txtDemangledName->setMaximumHeight(30);
    txtDemangledName->setReadOnly(true);
    horizontalLayout->addWidget(lblMangledName);
    horizontalLayout->addWidget(txtMangledName);
    horizontalLayout->setContentsMargins(10, 5, 10, 5);
    horizontalLayout2->addWidget(lblDemangledName);
    horizontalLayout2->addWidget(txtDemangledName);
    horizontalLayout2->setContentsMargins(10, 5, 10, 5);

    layout2->addLayout(horizontalLayout);
    layout2->addLayout(horizontalLayout2);
    layout2->addWidget(assemblyEditor);
    layout2->setMargin(0);

    QStringList addressTypes;

    addressTypes.append("VA");
    addressTypes.append("RVA");
    addressTypes.append("File offset");

    cbAddressTypes->addItems(addressTypes);
    cbAddressTypes2->addItems(addressTypes);

    tableView->setModel(proxyModel);
    lblFindItem->setText("Find item:");
    txtFindItem->setMaximumHeight(30);
    horizontalLayout3->addWidget(lblFindItem);
    horizontalLayout3->addWidget(txtFindItem);
    horizontalLayout3->setContentsMargins(10, 5, 10, 5);

    //listView->setMinimumWidth(256);
    lvVTables->setModel(model);
    tvVTables->setModel(proxyModel2);

    QFont font;

    font.setFamily("Consolas");
    font.setFixedPitch(true);
    font.setPointSize(14);

    const int tabStop = 4;
    QFontMetrics metrics(font);

    plainTextEdit->setTabStopWidth(tabStop * metrics.width(' '));

    ui.splitter->setStretchFactor(0, 0);
    ui.splitter->setStretchFactor(1, 1);

    plainTextEdit->setFont(font);
    plainTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    layout3->addLayout(horizontalLayout3);
    layout3->addWidget(tableView);
    layout3->setMargin(0);

    QSplitter* splitter = new QSplitter(this);

    splitter->addWidget(lvVTables);
    splitter->addWidget(tvVTables);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    layout4->addWidget(splitter);
    layout4->setMargin(0);

    layout5->addWidget(plainTextEdit, 0);
    layout5->setMargin(0);

    lblAddress->setText("Address:");
    txtAddress->setMaximumHeight(30);
    horizontalLayout4->addWidget(lblAddress);
    horizontalLayout4->addWidget(txtAddress);
    horizontalLayout4->addWidget(cbAddressTypes);
    horizontalLayout4->setContentsMargins(10, 5, 10, 5);

    layout6->addLayout(horizontalLayout4);
    layout6->addWidget(assemblyEditor2);
    layout6->setMargin(0);

    layout7->addWidget(assemblyEditor3);
    layout7->setMargin(0);
    layout7->setSpacing(0);

    lblMangledName2->setText("Mangled name:");
    lblDemangledName2->setText("Demangled name:");
    txtMangledName2->setMaximumHeight(30);
    txtDemangledName2->setMaximumHeight(30);
    txtDemangledName2->setReadOnly(true);
    horizontalLayout5->addWidget(lblMangledName2);
    horizontalLayout5->addWidget(txtMangledName2);
    horizontalLayout5->setContentsMargins(10, 5, 10, 5);
    horizontalLayout6->addWidget(lblDemangledName2);
    horizontalLayout6->addWidget(txtDemangledName2);
    horizontalLayout6->setContentsMargins(10, 5, 10, 5);

    layout8->addLayout(horizontalLayout5);
    layout8->addLayout(horizontalLayout6);
    layout8->setMargin(0);

    lblVirtualAddress->setText("Virtual address:");
    lblRelativeVirtualAddress->setText("Relative virtual address:");
    lblFileOffset->setText("File offset:");
    txtVirtualAddress->setMaximumHeight(30);
    txtRelativeVirtualAddress->setMaximumHeight(30);
    txtFileOffset->setMaximumHeight(30);
    horizontalLayout7->addWidget(lblVirtualAddress);
    horizontalLayout7->addWidget(txtVirtualAddress);
    horizontalLayout7->setContentsMargins(10, 5, 10, 5);
    horizontalLayout8->addWidget(lblRelativeVirtualAddress);
    horizontalLayout8->addWidget(txtRelativeVirtualAddress);
    horizontalLayout8->setContentsMargins(10, 5, 10, 5);
    horizontalLayout9->addWidget(lblFileOffset);
    horizontalLayout9->addWidget(txtFileOffset);
    horizontalLayout9->setContentsMargins(10, 5, 10, 5);

    layout9->addLayout(horizontalLayout7);
    layout9->addLayout(horizontalLayout8);
    layout9->addLayout(horizontalLayout9);
    layout9->setMargin(0);

    lblAddress2->setText("Address:");
    txtAddress2->setMaximumHeight(30);
    horizontalLayout10->addWidget(lblAddress2);
    horizontalLayout10->addWidget(txtAddress2);
    horizontalLayout10->addWidget(cbAddressTypes2);
    horizontalLayout10->setContentsMargins(10, 5, 10, 5);
    layout10->addLayout(horizontalLayout10);
    layout10->addWidget(pseudoCodeEditor);
    layout10->setMargin(0);

    layout11->addWidget(pseudoCodeEditor2);
    layout11->setMargin(0);
    layout11->setSpacing(0);

    stackedLayout = new QStackedLayout(this);

    QWidget* widget1 = new QWidget(this);
    QWidget* widget2 = new QWidget(this);
    QWidget* widget3 = new QWidget(this);
    QWidget* widget4 = new QWidget(this);
    QWidget* widget5 = new QWidget(this);
    QWidget* widget6 = new QWidget(this);
    QWidget* widget7 = new QWidget(this);
    QWidget* widget8 = new QWidget(this);
    QWidget* widget9 = new QWidget(this);
    QWidget* widget10 = new QWidget(this);
    QWidget* widget11 = new QWidget(this);

    widget1->setLayout(layout1);
    widget2->setLayout(layout2);
    widget3->setLayout(layout3);
    widget4->setLayout(layout4);
    widget5->setLayout(layout5);
    widget6->setLayout(layout6);
    widget7->setLayout(layout7);
    widget8->setLayout(layout8);
    widget9->setLayout(layout9);
    widget10->setLayout(layout10);
    widget11->setLayout(layout11);

    stackedLayout->addWidget(widget1);
    stackedLayout->addWidget(widget2);
    stackedLayout->addWidget(widget3);
    stackedLayout->addWidget(widget4);
    stackedLayout->addWidget(widget5);
    stackedLayout->addWidget(widget6);
    stackedLayout->addWidget(widget7);
    stackedLayout->addWidget(widget8);
    stackedLayout->addWidget(widget9);
    stackedLayout->addWidget(widget10);
    stackedLayout->addWidget(widget11);

    ui.tab->setLayout(stackedLayout);
    stackedLayout->setCurrentIndex(0);
}

void PDBExplorer::OpenActionTriggered()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/PDBExplorer.ini", QSettings::IniFormat);
    QString filePath = QFileDialog::getOpenFileName(this, "Open File...", settings.value("LastDirectory").toString(),
        "PDB Files (*.pdb);;All Files (*)");

    if (!filePath.isEmpty())
    {
        QFileInfo fileInfo(filePath);
        QString currentDirectory = fileInfo.absolutePath();

        settings.setValue("LastDirectory", currentDirectory);
        options.lastDirectory = currentDirectory;

        OpenFile(filePath);
    }

    this->filePath = filePath;
}

void PDBExplorer::ExitActionTriggered()
{
    close();
}

void PDBExplorer::OptionsActionTriggered()
{
    bool optionsChanged = false;

    OptionsDialog optionsDialog(this, &options, &optionsChanged);

    optionsDialog.exec();

    if (optionsChanged)
    {
        pdb->ClearElements();
    }
}

void PDBExplorer::ExportAllTypesActionTriggered()
{
    pdb->SetProcessType(ProcessType::exportAllUDTsAndEnums);

    PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::exportAllUDTsAndEnums);
    pdbProcessDialog.exec();
}

void PDBExplorer::ClearCacheActionTriggered()
{
    pdb->ClearElements();
}

void PDBExplorer::TxtSearchSymbolTextChanged(const QString& text)
{
    if (symbolRecords.size() < 40000)
    {
        symbolsViewProxyModel->setFilterRegExp(text);
        symbolsViewProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        symbolsViewProxyModel->setFilterKeyColumn(1);
    }
}

void PDBExplorer::BtnSearchSymbolClicked()
{
    symbolsViewProxyModel->setFilterRegExp(ui.txtSearchSymbol->text());
    symbolsViewProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    symbolsViewProxyModel->setFilterKeyColumn(1);
}

void PDBExplorer::OpenFile(const QString& filePath)
{
    QString filePathWithoutExtenstion = filePath;
    filePathWithoutExtenstion = filePathWithoutExtenstion.remove(filePath.length() - 3, 3);
    bool fileExists = false;

    if (QFileInfo::exists(QString("%1exe").arg(filePathWithoutExtenstion)))
    {
        if (peHeaderParser->ReadPEHeader(QString("%1exe").arg(filePathWithoutExtenstion)))
        {
            DisplayStatusMessage("Header parsed successfully.");
        }

        fileExists = true;
    }
    else if (QFileInfo::exists(QString("%1dll").arg(filePathWithoutExtenstion)))
    {
        if (peHeaderParser->ReadPEHeader(QString("%1dll").arg(filePathWithoutExtenstion)))
        {
            DisplayStatusMessage("Header parsed successfully.");
        }

        fileExists = true;
    }
    else
    {
        DisplayStatusMessage("EXE/DLL File is missing.");
    }

    if (fileExists)
    {
        DisplayFileInfo(filePath);
    }

    QAbstractItemModel* symbolsViewOldModel = symbolsViewProxyModel->sourceModel();
    QAbstractItemModel* oldModel = proxyModel->sourceModel();
    QAbstractItemModel* oldModel2 = proxyModel2->sourceModel();

    if (symbolsViewOldModel)
    {
        symbolsViewProxyModel->setSourceModel(0);

        delete symbolsViewOldModel;
    }

    if (oldModel)
    {
        proxyModel->setSourceModel(0);

        delete oldModel;
    }

    if (oldModel2)
    {
        proxyModel2->setSourceModel(0);

        delete oldModel2;
    }

    ui.txtSearchSymbol->clear();
    codeEditor->setText("");
    assemblyEditor->setText("");
    txtFindItem->clear();
    txtMangledName->clear();
    txtDemangledName->clear();
    txtAddress->clear();
    pdb->ClearElements();

    if (!ui.cbDisplayOptions->isEnabled())
    {
        ui.cbDisplayOptions->setEnabled(true);
    }

    if (pdb->ReadFromFile(filePath))
    {
        DisplayStatusMessage("PDB opened successfully.");

        ProcessType processType = GetProcessType();
        PDBProcessDialog pdbProcessDialog(this, pdb, processType);

        pdbProcessDialog.exec();

        if (symbolRecords.size() > 40000)
        {
            ui.btnSearchSymbol->setVisible(true);
        }

        switch (processType)
        {
        case ProcessType::importUDTsAndEnums:
            AddSymbolsToList();

            break;
        case ProcessType::importVariables:
            AddDataSymbolsToList(&variables);

            break;
        case ProcessType::importFunctions:
            AddFunctionSymbolsToList(&functions);

            break;
        case ProcessType::importPublicSymbols:
            AddPublicSymbolsToList();

            break;
        }

        isFileOpened = true;
    }
    else
    {
        DisplayStatusMessage(QString("%1: %2").arg("Can't open file").arg(filePath));
    }
}

ProcessType PDBExplorer::GetProcessType()
{
    ProcessType processType;

    switch (ui.cbSymbolTypes->currentIndex())
    {
    case 0:
        processType = ProcessType::importUDTsAndEnums;

        break;
    case 1:
        processType = ProcessType::importVariables;

        break;
    case 2:
        processType = ProcessType::importFunctions;

        break;
    case 3:
        processType = ProcessType::importPublicSymbols;

        break;
    }

    return processType;
}

void PDBExplorer::DisplayFileInfo(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString fileName(fileInfo.fileName());
    QString fileNameWithoutExtension = fileName.mid(0, fileName.length() - 4);

    setWindowTitle(fileNameWithoutExtension);

    pdb->SetFileNameWithoutExtension(fileNameWithoutExtension);
    pdb->SetWindowTitle(fileName);
    pdb->SetFilePath(filePath);

    machineType = peHeaderParser->GetMachineType();
    pdb->SetMachineType(machineType);
}

void PDBExplorer::TVSymbolsClicked(const QModelIndex& index)
{
    Q_UNUSED(index);

    HandleTableViewEvent();
}

void PDBExplorer::HandleTableViewEvent()
{
    int currentIndex = ui.cbDisplayOptions->currentIndex();

    switch (currentIndex)
    {
    case 0:
    {
        if (ui.cbSymbolTypes->currentIndex() == 0)
        {
            DisplayHeaderCode();
        }
        else if (ui.cbSymbolTypes->currentIndex() == 1)
        {
            DisplayVariableInfo();
        }
        else if (ui.cbSymbolTypes->currentIndex() == 2)
        {
            DisplayFunctionInfo();
        }
        else if (ui.cbSymbolTypes->currentIndex() == 3)
        {
            DisplayPublicSymbolInfo();
        }

        break;
    }
    case 1:
        DisplayCPPCode();

        break;
    case 2:
        DisplayStructureView();

        break;
    case 3:
        DisplayFunctionOffsets();

        break;
    case 4:
        DisplayVTables();

        break;
    case 5:
        DisplayUDTLayout();

        break;
    case 6:
        DisplayVTablesLayout();

        break;
    case 7:
        DisplayMSVCLayout();

        break;
    }

    DisplayStatusMessage("Done.");
}

SymbolRecord PDBExplorer::GetSelectedSymbolRecord()
{
    SymbolRecord symbolRecord = {};
    QItemSelectionModel* selectionModel = ui.tvSymbols->selectionModel();

    if (selectionModel)
    {
        QModelIndex index = selectionModel->currentIndex();

        if (index.row() == -1)
        {
            return symbolRecord;
        }

        symbolRecord.id = index.sibling(index.row(), 0).data().toUInt();
        symbolRecord.typeName = index.sibling(index.row(), 1).data().toString();
        symbolRecord.type = static_cast<SymbolType>(index.sibling(index.row(), 0).data(Qt::UserRole).toUInt());

        return symbolRecord;
    }

    return symbolRecord;
}

void PDBExplorer::HandleUDTAndEnumType()
{
    ui.chkClasses->setEnabled(true);
    ui.chkStructs->setEnabled(true);
    ui.chkUnions->setEnabled(true);
    ui.chkEnums->setEnabled(true);

    ui.chkMember->setEnabled(false);
    ui.chkStatic->setEnabled(false);
    ui.chkGlobal->setEnabled(false);

    ui.cbDisplayOptions->setItemText(0, "Header code");
    ui.cbDisplayOptions->setCurrentIndex(0);
    ui.tabWidget->setTabText(0, "Header code");

    EnableUDTAndEnumOptions();

    if (symbolRecords.size() > 0)
    {
        AddSymbolsToList();
    }
}

void PDBExplorer::HandleDataType()
{
    ui.chkClasses->setEnabled(false);
    ui.chkStructs->setEnabled(false);
    ui.chkUnions->setEnabled(false);
    ui.chkEnums->setEnabled(false);

    ui.chkMember->setEnabled(false);
    ui.chkStatic->setEnabled(true);
    ui.chkGlobal->setEnabled(true);

    ui.cbDisplayOptions->setItemText(0, "Variable info");
    ui.cbDisplayOptions->setCurrentIndex(0);
    ui.tabWidget->setTabText(0, "Variable info");
    stackedLayout->setCurrentIndex(4);

    DisableUDTAndEnumOptions();

    if (isFileOpened)
    {
        if (variables.count() == 0)
        {
            PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importVariables);

            pdbProcessDialog.exec();
        }

        AddDataSymbolsToList(&variables);
    }
}

void PDBExplorer::HandleFunctionType()
{
    ui.chkClasses->setEnabled(false);
    ui.chkStructs->setEnabled(false);
    ui.chkUnions->setEnabled(false);
    ui.chkEnums->setEnabled(false);

    ui.chkMember->setEnabled(true);
    ui.chkStatic->setEnabled(true);
    ui.chkGlobal->setEnabled(true);

    ui.cbDisplayOptions->setItemText(0, "Function info");
    ui.cbDisplayOptions->setCurrentIndex(0);
    ui.tabWidget->setTabText(0, "Function info");
    stackedLayout->setCurrentIndex(4);

    DisableUDTAndEnumOptions();

    if (isFileOpened)
    {
        if (functions.count() == 0)
        {
            PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importFunctions);

            pdbProcessDialog.exec();
        }

        AddFunctionSymbolsToList(&functions);
    }
}

void PDBExplorer::HandlePublicSymbolType()
{
    ui.chkClasses->setEnabled(false);
    ui.chkStructs->setEnabled(false);
    ui.chkUnions->setEnabled(false);
    ui.chkEnums->setEnabled(false);

    ui.chkMember->setEnabled(true);
    ui.chkStatic->setEnabled(true);
    ui.chkGlobal->setEnabled(true);

    ui.cbDisplayOptions->setItemText(0, "Public symbol info");
    ui.cbDisplayOptions->setCurrentIndex(0);
    ui.tabWidget->setTabText(0, "Public symbol info");
    stackedLayout->setCurrentIndex(4);

    DisableUDTAndEnumOptions();

    if (isFileOpened)
    {
        if (publicSymbols.count() == 0)
        {
            PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importPublicSymbols);

            pdbProcessDialog.exec();
        }

        AddPublicSymbolsToList();
    }
}

void PDBExplorer::EnableUDTAndEnumOptions()
{
    int count = ui.cbDisplayOptions->count();

    if (count > 0)
    {
        stackedLayout->setCurrentIndex(0);

        QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.cbDisplayOptions->model());

        for (int i = 1; i < 8; i++)
        {
            model->item(i)->setEnabled(true);
        }
    }
}

void PDBExplorer::DisableUDTAndEnumOptions()
{
    QStandardItemModel* model = static_cast<QStandardItemModel*>(ui.cbDisplayOptions->model());

    for (int i = 1; i < 8; i++)
    {
        model->item(i)->setEnabled(false);
    }
}

void PDBExplorer::ChkClassesToggled()
{
    if (ui.chkClasses->isChecked())
    {
        AddSymbolsToList(SymbolType::classType);
    }
    else
    {
        if (!ui.chkStructs->isChecked() &&
            !ui.chkUnions->isChecked() &&
            !ui.chkEnums->isChecked())
        {
            AddSymbolsToList();
        }
        else
        {
            RemoveSymbolsFromList(SymbolType::classType);
        }
    }
}

void PDBExplorer::ChkStructsToggled()
{
    if (ui.chkStructs->isChecked())
    {
        AddSymbolsToList(SymbolType::structType);
    }
    else
    {
        if (!ui.chkClasses->isChecked() &&
            !ui.chkUnions->isChecked() &&
            !ui.chkEnums->isChecked())
        {
            AddSymbolsToList();
        }
        else
        {
            RemoveSymbolsFromList(SymbolType::structType);
        }
    }
}

void PDBExplorer::ChkUnionsToggled()
{
    if (ui.chkUnions->isChecked())
    {
        AddSymbolsToList(SymbolType::unionType);
    }
    else
    {
        if (!ui.chkClasses->isChecked() &&
            !ui.chkStructs->isChecked() &&
            !ui.chkEnums->isChecked())
        {
            AddSymbolsToList();
        }
        else
        {
            RemoveSymbolsFromList(SymbolType::unionType);
        }
    }
}

void PDBExplorer::ChkEnumsToggled()
{
    if (ui.chkEnums->isChecked())
    {
        AddSymbolsToList(SymbolType::enumType);
    }
    else
    {
        if (!ui.chkClasses->isChecked() &&
            !ui.chkStructs->isChecked() &&
            !ui.chkUnions->isChecked())
        {
            AddSymbolsToList();
        }
        else
        {
            RemoveSymbolsFromList(SymbolType::enumType);
        }
    }
}

void PDBExplorer::ChkGlobalToggled()
{
    if (ui.chkGlobal->isChecked())
    {
        if (ui.cbSymbolTypes->currentIndex() == 1)
        {
            QHash<QString, DWORD> variables;

            for (auto it = this->variables.begin(); it != this->variables.end(); it++)
            {
                IDiaSymbol* symbol;

                if (pdb->GetSymbolByID(it.value(), &symbol))
                {
                    DWORD kind;
                    DataKind dataKind;

                    symbol->get_dataKind(&kind);
                    dataKind = static_cast<DataKind>(kind);

                    if (dataKind == DataIsGlobal || dataKind == DataIsConstant)
                    {
                        variables.insert(it.key(), it.value());
                    }

                    symbol->Release();
                }
            }

            AddDataSymbolsToList(&variables, true);
        }
        else if (ui.cbSymbolTypes->currentIndex() == 2)
        {
            QHash<QString, DWORD> functions;

            for (auto it = this->functions.begin(); it != this->functions.end(); it++)
            {
                IDiaSymbol* symbol;

                if (pdb->GetSymbolByID(it.value(), &symbol))
                {
                    IDiaSymbol* parentClass;
                    BSTR bString = nullptr;
                    bool isMemberFunction = false;

                    if (symbol->get_classParent(&parentClass) == S_OK)
                    {
                        parentClass->Release();

                        isMemberFunction = true;
                    }

                    if (!isMemberFunction)
                    {
                        functions.insert(it.key(), it.value());
                    }

                    symbol->Release();
                }
            }

            AddFunctionSymbolsToList(&functions, true, false, false);
        }
    }
    else
    {
        if (!ui.chkMember->isChecked() &&
            !ui.chkStatic->isChecked() &&
            !ui.chkGlobal->isChecked())
        {
            if (ui.cbSymbolTypes->currentIndex() == 1)
            {
                AddDataSymbolsToList(&variables);
            }
            else if (ui.cbSymbolTypes->currentIndex() == 2)
            {
                AddFunctionSymbolsToList(&functions);
            }
        }
        else
        {
            RemoveSymbolsFromList(0);
        }
    }
}

void PDBExplorer::ChkStaticToggled()
{
    if (ui.chkStatic->isChecked())
    {
        if (ui.cbSymbolTypes->currentIndex() == 1)
        {
            QHash<QString, DWORD> variables;

            for (auto it = this->variables.begin(); it != this->variables.end(); it++)
            {
                IDiaSymbol* symbol;

                if (pdb->GetSymbolByID(it.value(), &symbol))
                {
                    DWORD kind;
                    DataKind dataKind;

                    symbol->get_dataKind(&kind);
                    dataKind = static_cast<DataKind>(kind);

                    if (dataKind == DataIsStaticMember || dataKind == DataIsFileStatic)
                    {
                        variables.insert(it.key(), it.value());
                    }

                    symbol->Release();
                }
            }

            AddDataSymbolsToList(&variables, false);
        }
        else if (ui.cbSymbolTypes->currentIndex() == 2)
        {
            QHash<QString, DWORD> functions;

            for (auto it = this->functions.begin(); it != this->functions.end(); it++)
            {
                IDiaSymbol* symbol;

                if (pdb->GetSymbolByID(it.value(), &symbol))
                {
                    BOOL isStatic = 0;

                    symbol->get_isStatic(&isStatic);

                    if (symbol->get_isStatic(&isStatic) == S_OK && isStatic)
                    {
                        functions.insert(it.key(), it.value());
                    }

                    symbol->Release();
                }
            }

            AddFunctionSymbolsToList(&functions, false, true, false);
        }
    }
    else
    {
        if (!ui.chkMember->isChecked() &&
            !ui.chkStatic->isChecked() &&
            !ui.chkGlobal->isChecked())
        {
            if (ui.cbSymbolTypes->currentIndex() == 1)
            {
                AddDataSymbolsToList(&variables);
            }
            else if (ui.cbSymbolTypes->currentIndex() == 2)
            {
                AddFunctionSymbolsToList(&functions);
            }
        }
        else
        {
            RemoveSymbolsFromList(1);
        }
    }
}

void PDBExplorer::ChkMemberToggled()
{
    if (ui.chkMember->isChecked())
    {
        QHash<QString, DWORD> functions;

        for (auto it = this->functions.begin(); it != this->functions.end(); it++)
        {
            IDiaSymbol* symbol;

            if (pdb->GetSymbolByID(it.value(), &symbol))
            {
                IDiaSymbol* parentClass;
                BSTR bString = nullptr;
                bool isMemberFunction = false;

                if (symbol->get_classParent(&parentClass) == S_OK)
                {
                    parentClass->Release();

                    isMemberFunction = true;
                }

                if (isMemberFunction)
                {
                    functions.insert(it.key(), it.value());
                }

                symbol->Release();
            }
        }

        AddFunctionSymbolsToList(&functions, false, false, true);
    }
    else
    {
        if (!ui.chkMember->isChecked() &&
            !ui.chkStatic->isChecked() &&
            !ui.chkGlobal->isChecked())
        {
            AddFunctionSymbolsToList(&functions);
        }
        else
        {
            RemoveSymbolsFromList(2);
        }
    }
}

void PDBExplorer::CbSymbolTypesCurrentIndexChanged(int index)
{
    switch (index)
    {
    case 0:
        HandleUDTAndEnumType();

        break;
    case 1:
    {
        HandleDataType();

        break;
    }
    case 2:
        HandleFunctionType();

        break;
    case 3:
        HandlePublicSymbolType();

        break;
    }
}

void PDBExplorer::CbDisplayOptionsCurrentIndexChanged(int index)
{
    if (!ui.tab->layout())
    {
        return;
    }

    ui.tabWidget->setTabText(0, ui.cbDisplayOptions->currentText());

    switch (index)
    {
    case 0:
    case 1:
    {
        if (ui.cbSymbolTypes->currentIndex() == 0)
        {
            stackedLayout->setCurrentIndex(0);
        }
        else if (ui.cbSymbolTypes->currentIndex() == 1)
        {
            stackedLayout->setCurrentIndex(4);
        }
        else if (ui.cbSymbolTypes->currentIndex() == 2)
        {
            stackedLayout->setCurrentIndex(4);
        }
        else if (ui.cbSymbolTypes->currentIndex() == 3)
        {
            stackedLayout->setCurrentIndex(4);
        }

        break;
    }
    case 2:
    case 3:
        stackedLayout->setCurrentIndex(2);

        break;
    case 4:
        stackedLayout->setCurrentIndex(3);

        break;
    case 5:
        stackedLayout->setCurrentIndex(2);

        break;
    case 6:
    case 7:
        stackedLayout->setCurrentIndex(4);

        break;
    case 8:
    {
        stackedLayout->setCurrentIndex(4);

        if (isFileOpened)
        {
            DisplayModulesInfo();
        }

        break;
    }
    case 9:
    {
        stackedLayout->setCurrentIndex(4);

        if (isFileOpened)
        {
            DisplayLinesInfo();
        }

        break;
    }
    }
}

void PDBExplorer::AddSymbolsToList()
{
    int count = symbolRecords.size();
    QStandardItemModel* model = new QStandardItemModel(count, 2, this);

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (int i = 0; i < count; i++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(symbolRecords[i].id), Qt::DisplayRole); //Reverse arguments (Qt::DisplayRole, symbolRecords[i].id))
        itemID->setData(static_cast<quint32>(symbolRecords[i].type), Qt::UserRole);
        model->setItem(i, 0, itemID);

        itemSymbol->setText(symbolRecords[i].typeName);
        itemSymbol->setData(symbolRecords[i].typeName, Qt::ToolTipRole);
        model->setItem(i, 1, itemSymbol);
    }

    symbolsViewProxyModel->setSourceModel(model);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::AddSymbolsToList(SymbolType symbolType)
{
    int row = 0;
    int count = symbolRecords.size();
    int rowCount = 0;

    if (ui.chkClasses->isChecked())
    {
        rowCount += pdb->GetCountOfClasses();
    }

    if (ui.chkStructs->isChecked())
    {
        rowCount += pdb->GetCountOfStructs();
    }

    if (ui.chkUnions->isChecked())
    {
        rowCount += pdb->GetCountOfUnions();
    }

    if (ui.chkEnums->isChecked())
    {
        rowCount += pdb->GetCountOfEnums();
    }

    QStandardItemModel* model = new QStandardItemModel(rowCount, 2, this);

    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (int i = 0; i < count; i++)
    {
        if (symbolRecords[i].type == symbolType)
        {
            QStandardItem* itemID = new QStandardItem();
            QStandardItem* itemSymbol = new QStandardItem();

            itemID->setData(static_cast<quint32>(symbolRecords[i].id), Qt::DisplayRole);
            itemID->setData(static_cast<quint32>(symbolRecords[i].type), Qt::UserRole);
            model->setItem(row, 0, itemID);

            itemSymbol->setText(symbolRecords[i].typeName);
            itemSymbol->setData(symbolRecords[i].typeName, Qt::ToolTipRole);
            model->setItem(row, 1, itemSymbol);

            row++;
        }
    }

    QStandardItemModel* standardItemModel = static_cast<QStandardItemModel*>(symbolsViewProxyModel->sourceModel());
    int rowsCount2 = standardItemModel->rowCount();

    if (count != rowsCount2)
    {
        for (int i = 0; i < rowsCount2; i++)
        {
            QStandardItem* itemID = standardItemModel->item(i);
            QStandardItem* itemSymbol = standardItemModel->item(i, 1);

            QStandardItem* itemID2 = new QStandardItem();
            QStandardItem* itemSymbol2 = new QStandardItem();

            quint32 id = itemID->data(Qt::DisplayRole).toUInt();
            quint32 type = itemID->data(Qt::UserRole).toUInt();
            QString typeName = itemSymbol->text();

            itemID2->setData(id, Qt::DisplayRole);
            itemID2->setData(type, Qt::UserRole);
            model->setItem(row, 0, itemID2);

            itemSymbol2->setText(typeName);
            itemSymbol2->setData(typeName, Qt::ToolTipRole);
            model->setItem(row, 1, itemSymbol2);

            row++;
        }
    }

    symbolsViewProxyModel->setSourceModel(model);
}

void PDBExplorer::AddDataSymbolsToList(QHash<QString, DWORD>* variables)
{
    int row = 0;
    int count = variables->count();
    QStandardItemModel* standardItemModel = new QStandardItemModel(count, 2, this);

    standardItemModel->setHeaderData(0, Qt::Horizontal, "ID");
    standardItemModel->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (auto it = variables->begin(); it != variables->end(); it++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(it.value()), Qt::DisplayRole);
        standardItemModel->setItem(row, 0, itemID);

        itemSymbol->setText(it.key());
        itemSymbol->setData(it.key(), Qt::ToolTipRole);
        standardItemModel->setItem(row, 1, itemSymbol);

        row++;
    }

    symbolsViewProxyModel->setSourceModel(standardItemModel);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::AddDataSymbolsToList(QHash<QString, DWORD>* variables, bool isGlobal)
{
    int row = 0;
    int count = variables->count();
    QStandardItemModel* standardItemModel = new QStandardItemModel(count, 2, this);

    standardItemModel->setHeaderData(0, Qt::Horizontal, "ID");
    standardItemModel->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (auto it = variables->begin(); it != variables->end(); it++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(it.value()), Qt::DisplayRole);

        if (isGlobal)
        {
            itemID->setData(0, Qt::UserRole);
        }
        else
        {
            itemID->setData(1, Qt::UserRole);
        }

        standardItemModel->setItem(row, 0, itemID);

        itemSymbol->setText(it.key());
        itemSymbol->setData(it.key(), Qt::ToolTipRole);
        standardItemModel->setItem(row, 1, itemSymbol);

        row++;
    }

    symbolsViewProxyModel->setSourceModel(standardItemModel);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::AddFunctionSymbolsToList(QHash<QString, DWORD>* functions)
{
    int row = 0;
    int count = functions->count();
    QStandardItemModel* standardItemModel = new QStandardItemModel(count, 2, this);

    standardItemModel->setHeaderData(0, Qt::Horizontal, "ID");
    standardItemModel->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (auto it = functions->begin(); it != functions->end(); it++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(it.value()), Qt::DisplayRole);
        standardItemModel->setItem(row, 0, itemID);

        itemSymbol->setText(it.key());
        itemSymbol->setData(it.key(), Qt::ToolTipRole);
        standardItemModel->setItem(row, 1, itemSymbol);

        row++;
    }

    symbolsViewProxyModel->setSourceModel(standardItemModel);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::AddFunctionSymbolsToList(QHash<QString, DWORD>* functions, bool isGlobal, bool isStatic, bool isMember)
{
    int row = 0;
    int count = functions->count();
    QStandardItemModel* standardItemModel = new QStandardItemModel(count, 2, this);

    standardItemModel->setHeaderData(0, Qt::Horizontal, "ID");
    standardItemModel->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (auto it = functions->begin(); it != functions->end(); it++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(it.value()), Qt::DisplayRole);

        if (isGlobal)
        {
            itemID->setData(0, Qt::UserRole);
        }
        else if (isStatic)
        {
            itemID->setData(1, Qt::UserRole);
        }
        else if (isMember)
        {
            itemID->setData(2, Qt::UserRole);
        }

        standardItemModel->setItem(row, 0, itemID);

        itemSymbol->setText(it.key());
        itemSymbol->setData(it.key(), Qt::ToolTipRole);
        standardItemModel->setItem(row, 1, itemSymbol);

        row++;
    }

    symbolsViewProxyModel->setSourceModel(standardItemModel);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::AddPublicSymbolsToList()
{
    int row = 0;
    int count = publicSymbols.count();
    QStandardItemModel* standardItemModel = new QStandardItemModel(count, 2, this);

    standardItemModel->setHeaderData(0, Qt::Horizontal, "ID");
    standardItemModel->setHeaderData(1, Qt::Horizontal, "Symbol Name");

    for (auto it = publicSymbols.begin(); it != publicSymbols.end(); it++)
    {
        QStandardItem* itemID = new QStandardItem();
        QStandardItem* itemSymbol = new QStandardItem();

        itemID->setData(static_cast<quint32>(it.value()), Qt::DisplayRole);
        standardItemModel->setItem(row, 0, itemID);

        itemSymbol->setText(it.key());
        itemSymbol->setData(it.key(), Qt::ToolTipRole);
        standardItemModel->setItem(row, 1, itemSymbol);

        row++;
    }

    symbolsViewProxyModel->setSourceModel(standardItemModel);
    symbolsViewProxyModel->setFilterRegExp("");
    symbolsViewProxyModel->setFilterKeyColumn(1);

    ui.tvSymbols->setColumnHidden(0, true);
    ui.tvSymbols->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void PDBExplorer::RemoveSymbolsFromList(SymbolType symbolType)
{
    QStandardItemModel* standardItemModel = static_cast<QStandardItemModel*>(symbolsViewProxyModel->sourceModel());
    int rowsCount = standardItemModel->rowCount();

    for (int i = rowsCount - 1; i >= 0; i--)
    {
        QStandardItem* itemID = standardItemModel->item(i);
        SymbolType type = static_cast<SymbolType>(itemID->data(Qt::UserRole).toUInt());

        if (type == symbolType)
        {
            standardItemModel->removeRow(i);
        }
    }

    //symbolsViewProxyModel->setSourceModel(standardItemModel);
}

void PDBExplorer::RemoveSymbolsFromList(const int type)
{
    QStandardItemModel* standardItemModel = static_cast<QStandardItemModel*>(symbolsViewProxyModel->sourceModel());
    int rowsCount = standardItemModel->rowCount();

    for (int i = rowsCount - 1; i >= 0; i--)
    {
        QStandardItem* itemID = standardItemModel->item(i);
        const int type2 = static_cast<quint32>(itemID->data(Qt::UserRole).toUInt());

        if (type == type2)
        {
            standardItemModel->removeRow(i);
        }
    }
}

void PDBExplorer::DisplayStatusMessage(const QString& message)
{
    ui.statusBar->showMessage(message, 3000);
}

void PDBExplorer::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction();
}

void PDBExplorer::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls())
    {
        QList<QUrl> urlList = mimeData->urls();

        if (urlList.count())
        {
            QString filePath = urlList.at(0).toLocalFile();

            QFileInfo fileInfoLink(filePath);

            if (fileInfoLink.isSymLink())
            {
                filePath = fileInfoLink.symLinkTarget();
            }

            OpenFile(filePath);
        }
    }
}

void PDBExplorer::DisplayHeaderCode()
{
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    QString message;
    Element element = pdb->GetElement(&symbolRecord);
    bool areSizesCorrect;

    if (element.elementType == ElementType::enumType)
    {
        areSizesCorrect = true;
    }
    else
    {
        areSizesCorrect = pdb->CheckIfChildrenSizesAreCorrect(const_cast<Element*>(&element), message);
    }

    if (areSizesCorrect)
    {
        QString elementInfo = pdb->GetElementInfo(&element);

        codeEditor->setText(elementInfo.toStdString().c_str());
    }
    else
    {
        QMessageBox::StandardButton messageBox = QMessageBox::critical(nullptr, "Error", message, QMessageBox::Ok);
    }
}

void PDBExplorer::DisplayCPPCode()
{
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord);
    QString cppCode = pdb->GenerateCPPCode(&element);
    QString name = element.udt.name;

    if (name.contains("<"))
    {
        name = name.mid(0, name.indexOf("<"));
    }

    if (name.contains("::"))
    {
        name = name.mid(0, name.indexOf("::"));
    }

    cppCode.prepend("#include \"BaseAddresses.h\"\r\n\r\n");
    cppCode.prepend("#include \"Function.h\"\r\n");
    cppCode.prepend(QString("#include \"%1.h\"\r\n").arg(name));

    codeEditor->setText(cppCode.toStdString().c_str());

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayVariableInfo()
{
    int count = variables.count();

    if (count == 0)
    {
        PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importVariables);
        pdbProcessDialog.exec();
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    QString output;
    IDiaSymbol* symbol;
    static const DataOptions dataOptions = { true };

    if (pdb->GetSymbolByID(variables[symbolRecord.typeName], &symbol))
    {
        Element element = pdb->GetElement(symbol);
        QString type = pdb->DataTypeToString(const_cast<Data*>(&element.data), &dataOptions);

        DWORD relativeVirtualAddress = element.data.relativeVirtualAddress;
        DWORD fileOffset = peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress);

        QString relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
        QString fileOffset2 = QString::number(fileOffset, 16).toUpper();
        QString size = QString::number(element.size, 16).toUpper();
        QString bitOffset = QString::number(element.bitOffset, 16).toUpper();
        QString dataKind = pdb->convertDataKindToString(element.data.dataKind);
        QString locationType = pdb->convertLocationTypeToString(element.data.locationType);

        output += QString("Type:                  %1\r\n").arg(type);
        output += QString("Name:                  %1\r\n").arg(element.data.name);
        output += QString("Virtual Offset:        0x%1\r\n").arg(relativeVirtualAddress2);
        output += QString("File Offset:           0x%1\r\n").arg(fileOffset2);
        output += QString("Size:                  0x%1\r\n").arg(size);
        output += QString("Bit offset:            0x%1\r\n").arg(bitOffset);
        output += QString("Number of bits:        %1\r\n").arg(element.numberOfBits);
        output += QString("Is compiler generated: %1\r\n").arg(ConvertBOOLToString(element.data.isCompilerGenerated));
        output += QString("Data kind:             %1\r\n").arg(dataKind);
        output += QString("Parent class name:     %1\r\n").arg(element.data.parentClassName);
        output += QString("Parent type:           %1\r\n").arg(element.data.parentType);
        output += QString("Location type:         %1\r\n").arg(locationType);

        plainTextEdit->setPlainText(output);

        symbol->Release();
    }

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayFunctionInfo()
{
    int count = functions.count();

    if (count == 0)
    {
        PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importFunctions);
        pdbProcessDialog.exec();
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    QString output;
    IDiaSymbol* symbol;

    if (pdb->GetSymbolByID(functions[symbolRecord.typeName], &symbol))
    {
        Element element = pdb->GetElement(symbol);
        Element element2 = {};

        if (!element.function.returnType1.isPointer &&
            !element.function.returnType1.isReference)
        {
            IDiaSymbol* symbol2;
            DWORD id = diaSymbols.value(element.function.returnType1.originalTypeName);

            if (pdb->GetSymbolByID(id, &symbol2))
            {
                element.function.isRVOApplied = pdb->CheckIfRVOIsAppliedToFunction(symbol2);
            }
        }

        static const FunctionOptions functionOptions = { true, false, true, false, true };
        QString functionPrototype = pdb->FunctionTypeToString(const_cast<Element*>(&element), &functionOptions);

        DWORD relativeVirtualAddress = element.function.relativeVirtualAddress;
        DWORD fileOffset = peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress);

        QString relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
        QString fileOffset2 = QString::number(fileOffset, 16).toUpper();
        QString size = QString::number(element.size, 16).toUpper();
        QString accessSpecifier = pdb->convertAccessSpecifierToString(element.function.access);
        QString callingConvention = pdb->convertCallingConventionToString(element.function.callingConvention);
        bool isPure = element.function.isVirtual && element.function.isPure;

        output += QString("Name:                          %1\r\n").arg(element.function.name);
        output += QString("Prototype:                     %1\r\n").arg(functionPrototype);

        if (element.function.isRVOApplied)
        {
            element2 = element;
            pdb->ApplyReturnValueOptimization(&element2);

            QString functionPrototype2 = pdb->FunctionTypeToString(const_cast<Element*>(&element2), &functionOptions);

            output += QString("Prototype after applying RVO:  %1\r\n").arg(functionPrototype2);
        }

        output += QString("Virtual offset:                0x%1\r\n").arg(relativeVirtualAddress2);
        output += QString("File offset:                   0x%1\r\n").arg(fileOffset2);
        output += QString("Address section:               %1\r\n").arg(element.function.addressSection);
        output += QString("Size:                          0x%1\r\n").arg(size);
        output += QString("Parent class name:             %1\r\n").arg(element.function.parentClassName);
        output += QString("Parent type:                   %1\r\n").arg(element.function.parentType);
        output += QString("Source file path:              %1\r\n").arg(pdb->GetSourceFilePath(symbol));
        output += QString("Access:                        %1\r\n").arg(accessSpecifier);
        output += QString("Calling convention:            %1\r\n").arg(callingConvention);
        output += QString("Is compiler generated:         %1\r\n").arg(ConvertBOOLToString(element.function.isCompilerGenerated));
        output += QString("Is naked:                      %1\r\n").arg(ConvertBOOLToString(element.function.isNaked));
        output += QString("Is static:                     %1\r\n").arg(ConvertBOOLToString(element.function.isStatic));
        output += QString("Is no inline:                  %1\r\n").arg(ConvertBOOLToString(element.function.isNoInline));
        output += QString("Is not reached:                %1\r\n").arg(ConvertBOOLToString(element.function.isNotReached));
        output += QString("Is no return:                  %1\r\n").arg(ConvertBOOLToString(element.function.isNoReturn));
        output += QString("Is const:                      %1\r\n").arg(ConvertBOOLToString(element.function.isConst));
        output += QString("Is volatile:                   %1\r\n").arg(ConvertBOOLToString(element.function.isVolatile));
        output += QString("Is virtual:                    %1\r\n").arg(ConvertBOOLToString(element.function.isVirtual));
        output += QString("Is pure:                       %1\r\n").arg(ConvertBOOLToString(isPure));
        output += QString("Is overriden:                  %1\r\n").arg(ConvertBOOLToString(element.function.isOverridden));
        output += QString("Is constructor:                %1\r\n").arg(ConvertBOOLToString(element.function.isConstructor));
        output += QString("Is default constructor:        %1\r\n").arg(ConvertBOOLToString(element.function.isDefaultConstructor));
        output += QString("Is destructor:                 %1\r\n").arg(ConvertBOOLToString(element.function.isDestructor));
        output += QString("Is copy constructor:           %1\r\n").arg(ConvertBOOLToString(element.function.isCopyConstructor));
        output += QString("Is copy assignment operator:   %1\r\n").arg(ConvertBOOLToString(element.function.isCopyAssignmentOperator));
        output += QString("Is cast operator:              %1\r\n").arg(ConvertBOOLToString(element.function.isCastOperator));
        output += QString("Is variadic:                   %1\r\n").arg(ConvertBOOLToString(element.function.isVariadic));
        output += QString("Is non implemented:            %1\r\n").arg(ConvertBOOLToString(element.function.isNonImplemented));
        output += QString("Is RVO applied:                %1\r\n").arg(ConvertBOOLToString(element.function.isRVOApplied));
        output += QString("Is Intro Virtual:              %1\r\n").arg(ConvertBOOLToString(element.function.isIntroVirtual));
        output += QString("Has alloca:                    %1\r\n").arg(ConvertBOOLToString(element.function.hasAlloca));
        output += QString("Has EH:                        %1\r\n").arg(ConvertBOOLToString(element.function.hasEH));
        output += QString("Has EHa:                       %1\r\n").arg(ConvertBOOLToString(element.function.hasEHa));
        output += QString("Has inl asm:                   %1\r\n").arg(ConvertBOOLToString(element.function.hasInlAsm));
        output += QString("Has long jump:                 %1\r\n").arg(ConvertBOOLToString(element.function.hasLongJump));
        output += QString("Has security checks:           %1\r\n").arg(ConvertBOOLToString(element.function.hasSecurityChecks));
        output += QString("Has SEH:                       %1\r\n").arg(ConvertBOOLToString(element.function.hasSEH));
        output += QString("Has set jump:                  %1\r\n").arg(ConvertBOOLToString(element.function.hasSetJump));
        output += QString("Has inl spec:                  %1\r\n").arg(ConvertBOOLToString(element.function.hasInlSpec));
        output += QString("Has optimized code debug info: %1\r\n").arg(ConvertBOOLToString(element.function.hasOptimizedCodeDebugInfo));
        output += QString("Far return:                    %1\r\n").arg(ConvertBOOLToString(element.function.farReturn));
        output += QString("Interrupt return:              %1\r\n").arg(ConvertBOOLToString(element.function.interruptReturn));
        output += QString("No stack ordering:             %1\r\n").arg(ConvertBOOLToString(element.function.noStackOrdering));

        if (element.function.parentClassName.length() > 0 && element.function.isVirtual)
        {
            SymbolRecord symbolRecord;

            symbolRecord.id = diaSymbols[element.function.originalParentClassName];
            symbolRecord.typeName = element.function.parentClassName;

            if (element.function.parentType == "class")
            {
                symbolRecord.type = SymbolType::classType;
            }
            else
            {
                symbolRecord.type = SymbolType::structType;
            }

            Element element2 = pdb->GetElement(&symbolRecord);
            int virtualFunctionChildrenCount = element2.virtualFunctionChildren.count();

            for (int i = 0; i < virtualFunctionChildrenCount; i++)
            {
                if (element2.virtualFunctionChildren.at(i).function.prototype == functionPrototype)
                {
                    int virtualBaseOffset = element2.virtualFunctionChildren.at(i).function.virtualBaseOffset;
                    int vTableIndex = element2.virtualFunctionChildren.at(i).function.virtualBaseOffset >> 2;
                    int indexOfVTable = element2.virtualFunctionChildren.at(i).function.indexOfVTable;
                    QString vTableName = element2.udt.vTableNames[indexOfVTable];

                    output += QString("Virtual base offset:           %1\r\n").arg(virtualBaseOffset);
                    output += QString("VTable index:                  %1\r\n").arg(vTableIndex);
                    output += QString("VTable name:                   %1\r\n").arg(vTableName);
                }
            }
        }

        static const DataOptions dataOptions = { true };
        QList<QString> functionParameters = element.function.parameters;
        QList<QString> functionParameterNames;

        /*if (element.function.isRVOApplied)
        {
            functionParameters.prepend(element2.function.parameters.at(0));
        }*/

        if (element.dataChildren.count() > 0 &&
            element.dataChildren.at(0).data.dataKind == DataIsObjectPtr)
        {
            functionParameters.prepend(pdb->DataTypeToString(&element.dataChildren.at(0).data, &dataOptions));
        }

        int n = 0;
        int parametersCount = functionParameters.count();
        int parameterNamesCount = element.dataChildren.count();

        if (parametersCount)
        {
            output += QString("\r\nParameters:\r\n");

            for (int i = 0; i < parametersCount; i++)
            {
                QString parameterType = functionParameters.at(i);
                QString parameterName;
                BOOL isCompilerGenerated = 0;
                QString offset;
                QString dataKind;
                QString location;

                if (parameterNamesCount > 0)
                {
                    if (n < parameterNamesCount)
                    {
                        parameterName = element.dataChildren.at(n).data.name;
                        isCompilerGenerated = element.dataChildren.at(n).data.isCompilerGenerated;
                        offset = QString::number(element.dataChildren.at(n).data.offset, 16).toUpper();
                        dataKind = pdb->convertDataKindToString(element.dataChildren.at(n).data.dataKind);
                        location = element.dataChildren.at(n).data.location;

                        n++;
                    }
                    else
                    {
                        parameterName = pdb->GenerateCustomParameterName(parameterType, i);
                    }
                }
                else
                {
                    parameterName = pdb->GenerateCustomParameterName(parameterType, i);
                }

                if (functionParameterNames.contains(parameterName))
                {
                    parameterName += QString("%1").arg(i);
                }

                functionParameterNames.append(parameterName);

                output += QString("\tType:                     %1\r\n").arg(parameterType);
                output += QString("\tName:                     %1\r\n").arg(parameterName);
                output += QString("\tIs compiler generated:    %1\r\n").arg(ConvertBOOLToString(isCompilerGenerated));
                output += QString("\tOffset:                   0x%1\r\n").arg(offset);
                output += QString("\tData kind:                %1\r\n").arg(dataKind);
                output += QString("\tLocation:                 %1\r\n").arg(location);

                if (i != parametersCount - 1)
                {
                    output += "\r\n";
                }
            }
        }

        int localVariablesCount = element.localVariables.count();

        if (localVariablesCount > 0)
        {
            output += QString("\r\nLocal variables:\r\n");

            for (int i = 0; i < localVariablesCount; i++)
            {
                QString variableType = pdb->DataTypeToString(&element.localVariables.at(i).data, &dataOptions);
                QString variableName = element.localVariables.at(i).data.name;
                BOOL isCompilerGenerated = element.localVariables.at(i).data.isCompilerGenerated;
                QString offset = QString::number(element.localVariables.at(i).data.offset, 16).toUpper();
                QString dataKind = pdb->convertDataKindToString(element.localVariables.at(i).data.dataKind);
                QString location = element.localVariables.at(i).data.location;

                output += QString("\tType:                     %1\r\n").arg(variableType);
                output += QString("\tName:                     %1\r\n").arg(variableName);
                output += QString("\tIs compiler generated:    %1\r\n").arg(ConvertBOOLToString(isCompilerGenerated));
                output += QString("\tOffset:                   0x%1\r\n").arg(offset);
                output += QString("\tData kind:                %1\r\n").arg(dataKind);
                output += QString("\tLocation:                 %1\r\n").arg(location);

                if (i != localVariablesCount - 1)
                {
                    output += "\r\n";
                }
            }
        }

        plainTextEdit->setPlainText(output);

        symbol->Release();
    }

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayPublicSymbolInfo()
{
    int count = publicSymbols.count();

    if (count == 0)
    {
        PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::importPublicSymbols);
        pdbProcessDialog.exec();
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    QString output;
    IDiaSymbol* symbol;

    if (pdb->GetSymbolByID(publicSymbols[symbolRecord.typeName], &symbol))
    {
        PublicSymbol publicSymbol = pdb->GetPublicSymbol(symbol);

        symbol->Release();

        DWORD relativeVirtualAddress = publicSymbol.relativeVirtualAddress;
        DWORD fileOffset = peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress);

        QString relativeVirtualAddress2 = QString::number(relativeVirtualAddress, 16).toUpper();
        QString fileOffset2 = QString::number(fileOffset, 16).toUpper();
        QString addressOffset = QString::number(publicSymbol.addressOffset, 16).toUpper();
        QString length = QString::number(publicSymbol.length, 16).toUpper();
        QString locationType = pdb->convertLocationTypeToString(publicSymbol.locationType);

        output += QString("Demangled name:     %1\r\n").arg(publicSymbol.undecoratedName);
        output += QString("Mangled name:       %1\r\n").arg(publicSymbol.decoratedName);
        output += QString("Virtual offset:     0x%1\r\n").arg(relativeVirtualAddress2);
        output += QString("File offset:        0x%1\r\n").arg(fileOffset2);
        output += QString("Address offset:     0x%1\r\n").arg(addressOffset);
        output += QString("Address section:    %1\r\n").arg(publicSymbol.addressSection);
        output += QString("Is in code:         %1\r\n").arg(ConvertBOOLToString(publicSymbol.isInCode));
        output += QString("Is function:        %1\r\n").arg(ConvertBOOLToString(publicSymbol.isFunction));
        output += QString("Is in managed code: %1\r\n").arg(ConvertBOOLToString(publicSymbol.isInManagedCode));
        output += QString("Is in MSIL code:    %1\r\n").arg(ConvertBOOLToString(publicSymbol.isInMSILCode));
        output += QString("Length:             0x%1\r\n").arg(length);
        output += QString("Location type:      %1\r\n").arg(locationType);

        plainTextEdit->setPlainText(output);
    }

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayStructureView()
{
    QAbstractItemModel* oldModel = proxyModel->sourceModel();

    if (oldModel)
    {
        proxyModel->setSourceModel(0);

        delete oldModel;
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord);
    pdb->JoinLists(&element);

    int count = element.children.size();
    QStandardItemModel* model = new QStandardItemModel(count, 4, this);

    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Type");
    model->setHeaderData(2, Qt::Horizontal, "Offset");
    model->setHeaderData(3, Qt::Horizontal, "Size");

    QString type = "";
    QString name = "";
    int row = 0;
    int baseClassOffset = 0;

    for (int i = 0; i < count; i++)
    {
        if (element.children.at(i).elementType == ElementType::baseClassType)
        {
            type = element.children.at(i).baseClass.name;
            name = QString("baseClass_%1").arg(i);

            AddItemToModel(name, type, baseClassOffset, element.children.at(i).baseClass.length, model, row++);

            baseClassOffset += element.children.at(i).baseClass.length;
        }
        else if (element.children.at(i).elementType == ElementType::dataType &&
            element.children.at(i).data.dataKind != DataIsStaticMember)
        {
            if (element.children.at(i).data.isFunctionPointer)
            {
                type = element.children.at(i).data.functionReturnType;
            }
            else
            {
                type = element.children.at(i).data.typeName;
            }

            if (element.children.at(i).data.isPointer)
            {
                for (int j = 0; j < element.children.at(i).data.pointerLevel; j++)
                {
                    type += "*";
                }
            }

            if (element.children.at(i).data.isReference)
            {
                type += "&";
            }

            name = element.children.at(i).data.name;

            DWORD size = element.children.at(i).size;

            if (element.children.at(i).numberOfBits > 0)
            {
                DWORD offset = element.children.at(i).offset;
                unsigned long numberOfBits = 0;
                int countOfNewVariables = 0;

                while (i < count &&
                    element.children.at(i).elementType == ElementType::dataType &&
                    element.children.at(i).numberOfBits != 0)
                {
                    numberOfBits += element.children.at(i).numberOfBits;

                    if (numberOfBits >= size * 8)
                    {
                        countOfNewVariables++;
                        numberOfBits = 0;
                    }

                    i++;
                }

                i--; //i++ in while loop and i++ in main for loop will skip one member

                for (int j = 0; j < countOfNewVariables; j++)
                {
                    offset += size * j;

                    AddItemToModel("Bitfield", type, offset, size, model, row++);
                }
            }
            else
            {
                AddItemToModel(name, type, element.children.at(i).offset, size, model, row++);
            }
        }
        else if (element.children.at(i).elementType == ElementType::dataType &&
            element.children.at(i).data.isPadding)
        {
            if (element.children.at(i).offset == 0)
            {
                continue;
            }

            type = element.children.at(i).data.typeName;
            name = element.children.at(i).data.name;

            AddItemToModel(name, type, element.children.at(i).offset, element.children.at(i).size, model, row++);
        }
        else if (element.children.at(i).elementType == ElementType::functionType)
        {
            break;
        }
    }

    model->removeRows(row, count - row);
    proxyModel->setSourceModel(model);

    tableView->setColumnWidth(0, 500);
    tableView->setColumnWidth(1, 250);
    tableView->setColumnWidth(2, 100);
    tableView->setColumnWidth(3, 120);

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayFunctionOffsets()
{
    QAbstractItemModel* oldModel = proxyModel->sourceModel();

    if (oldModel)
    {
        proxyModel->setSourceModel(0);

        delete oldModel;
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord);
    int count = element.virtualFunctionChildren.count() + element.nonVirtualFunctionChildren.count();
    QStandardItemModel* model = new QStandardItemModel(count, 3, this);

    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Virtual Offfset");
    model->setHeaderData(2, Qt::Horizontal, "File Offset");

    int row = 0;

    AddFunctionsToModel(&element, model, &row, element.udt.name);

    model->removeRows(row, count - row);
    proxyModel->setSourceModel(model);

    tableView->setColumnWidth(0, 500);
    tableView->setColumnWidth(1, 250);
    tableView->setColumnWidth(2, 100);

    DisplayStatusMessage("Done.");
}

void PDBExplorer::AddFunctionsToModel(const Element* element, QStandardItemModel* model, int* row, const QString& currentUDTName)
{
    int udtChildrenCount = element->udtChildren.count();
    int functionChildrenCount = element->virtualFunctionChildren.count() + element->nonVirtualFunctionChildren.count();
    QList<Element> children;

    children.append(element->virtualFunctionChildren);
    children.append(element->nonVirtualFunctionChildren);

    for (int i = 0; i < udtChildrenCount; i++)
    {
        AddFunctionsToModel(&element->udtChildren.at(i), model, row, QString("::%1").arg(element->udtChildren.at(i).udt.name));
    }

    for (int i = 0; i < functionChildrenCount; i++)
    {
        Element element2 = children.at(i);
        static const FunctionOptions functionOptions = { true, false, false, false };
        QString name = pdb->FunctionTypeToString(const_cast<Element*>(&element2), &functionOptions);

        DWORD relativeVirtualAddress = children.at(i).function.relativeVirtualAddress;

        if (relativeVirtualAddress == 0)
        {
            continue;
        }

        DWORD fileOffset = peHeaderParser->ConvertVAToFileOffset(relativeVirtualAddress);

        AddItemToModel(name, relativeVirtualAddress, fileOffset, model, *row);

        (*row)++;
    }
}

void PDBExplorer::GetVTables(const Element* element, QStringList* vTables)
{
    QMap<int, QString>::const_iterator it;
    int i = 0;

    for (it = element->udt.vTableNames.begin(); it != element->udt.vTableNames.end(); it++)
    {
        QHash<QString, int> vTable = pdb->GetVTable(element->udt.vTableNames.value(it.key()));
        QString vTable2 = QString("VTable %1 (%2 - %3)").arg(i++).arg(it.value()).arg(vTable.count());

        vTables->append(vTable2);
    }
}

void PDBExplorer::DisplayVTables()
{
    QStringList vTables;
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    element = pdb->GetElement(&symbolRecord, true);

    if (element.udt.numOfVTables > 0)
    {
        if (!element.udt.hasBaseClass)
        {
            QString vTable = QString("VTable 1 (%1)").arg(element.udt.countOfVirtualFunctions);

            vTables.append(vTable);
        }
        else
        {
            GetVTables(&element, &vTables);
        }
    }

    model->setStringList(vTables);

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayVTable(int vTableNum)
{
    QAbstractItemModel* oldModel = proxyModel2->sourceModel();

    if (oldModel)
    {
        proxyModel2->setSourceModel(0);

        delete oldModel;
    }

    int row = 0;
    QMap<int, QString> virtualFunctions;

    if (element.udt.hasBaseClass)
    {
        QHash<QString, int>::const_iterator it;
        QHash<QString, int> vTable = pdb->GetVTable(element.udt.vTableNames.value(vTableNum));
        QHash<QString, QString> vTableFunctions = pdb->getFunctionPrototypes(element.udt.vTableNames.value(vTableNum));

        for (it = vTable.begin(); it != vTable.end(); it++)
        {
            QHash<QString, QString>::const_iterator it2 = vTableFunctions.find(it.key());

            if (it2 != vTableFunctions.end())
            {
                QString functionName = it2.value().mid(it2.value().lastIndexOf("::") + 2);
                QString functionPrototype = it.key();

                functionPrototype.replace(functionName, it2.value());

                virtualFunctions.insert(it.value(), functionPrototype);
            }
        }
    }
    else
    {
        int count = element.virtualFunctionChildren.count();

        for (int i = 0; i < count; i++)
        {
            if (element.virtualFunctionChildren.at(i).elementType == ElementType::functionType &&
                element.virtualFunctionChildren.at(i).function.isVirtual)
            {
                int offset = element.virtualFunctionChildren.at(i).function.virtualBaseOffset;
                static const FunctionOptions functionOptions = { true, false, true, false };
                QString functionPrototype = pdb->FunctionTypeToString(const_cast<Element*>(&element.virtualFunctionChildren.at(i)),
                    &functionOptions);

                virtualFunctions.insert(offset, functionPrototype);
            }
        }
    }

    QStandardItemModel* model = new QStandardItemModel(virtualFunctions.count(), 3, this);

    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Index");
    model->setHeaderData(2, Qt::Horizontal, "Offset");

    QMap<int, QString>::const_iterator it;

    for (it = virtualFunctions.begin(); it != virtualFunctions.end(); it++)
    {
        AddItemToModel(it.value(), it.key(), model, row++);
    }

    proxyModel2->setSourceModel(model);

    //tvVTables->resizeRowsToContents();
    //tvVTables->resizeColumnsToContents();
    tvVTables->setColumnWidth(0, 500);
    tvVTables->setColumnWidth(1, 50);
    tvVTables->setColumnWidth(2, 50);

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayUDTLayout()
{
    QAbstractItemModel* oldModel = proxyModel->sourceModel();

    if (oldModel)
    {
        proxyModel->setSourceModel(0);

        delete oldModel;
    }

    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord);
    pdb->JoinLists(&element);

    Element newElement = {};
    quint64 offset = 0;
    quint32 alignmentNum = 0;

    pdb->FlattenUDT(&element, &newElement, &offset, &alignmentNum);

    int count = newElement.children.size();
    QStandardItemModel* model = new QStandardItemModel(count, 4, this);

    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Type");
    model->setHeaderData(2, Qt::Horizontal, "Offset");
    model->setHeaderData(3, Qt::Horizontal, "Size");

    QString type = "";
    QString name = "";
    int row = 0;
    int baseClassOffset = 0;

    for (int i = 0; i < count; i++)
    {
        if (newElement.children.at(i).elementType == ElementType::dataType &&
            newElement.children.at(i).data.dataKind != DataIsStaticMember)
        {
            if (newElement.children.at(i).data.isFunctionPointer)
            {
                type = newElement.children.at(i).data.functionReturnType;
            }
            else
            {
                type = newElement.children.at(i).data.typeName;
            }

            if (newElement.children.at(i).data.isPointer)
            {
                for (int j = 0; j < newElement.children.at(i).data.pointerLevel; j++)
                {
                    type += "*";
                }
            }

            if (newElement.children.at(i).data.isReference)
            {
                type += "&";
            }

            name = newElement.children.at(i).data.name;

            DWORD size = newElement.children.at(i).size;
            AddItemToModel(name, type, newElement.children.at(i).offset, size, model, row++);
        }
    }

    proxyModel->setSourceModel(model);

    tableView->setColumnWidth(0, 500);
    tableView->setColumnWidth(1, 250);
    tableView->setColumnWidth(2, 100);
    tableView->setColumnWidth(3, 120);

    DisplayStatusMessage("Done.");
}

void PDBExplorer::DisplayVTablesLayout()
{
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord, true);
    pdb->JoinLists(&element);

    QString layout;
    quint64 offset = 0;

    pdb->GetVTablesLayout(&element, layout, &offset);
    layout += pdb->GetVirtualFunctionsInfo(&element);

    plainTextEdit->setPlainText(layout);
}

void PDBExplorer::DisplayMSVCLayout()
{
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    Element element = pdb->GetElement(&symbolRecord, true);
    pdb->JoinLists(&element);

    QString layout;
    quint64 offset = 0;

    pdb->GetMSVCLayout(&element, layout, &offset);
    plainTextEdit->setPlainText(layout);
}

void PDBExplorer::DisplayModulesInfo()
{
    plainTextEdit->setPlainText(pdb->GetModules());
}

void PDBExplorer::DisplayLinesInfo()
{
    plainTextEdit->setPlainText(pdb->GetLines());
}

void PDBExplorer::AddItemToModel(const QString& name, const QString& type, DWORD offset, DWORD size, QStandardItemModel* model, int row)
{
    QStandardItem* itemName = new QStandardItem();

    itemName->setText(name);
    model->setItem(row, 0, itemName);

    QStandardItem* itemType = new QStandardItem();

    itemType->setText(type);
    model->setItem(row, 1, itemType);

    QStandardItem* itemOffset = new QStandardItem();
    QString offset2 = QString::number(offset, 16).toUpper();

    itemOffset->setText(QString("0x%1").arg(offset2));
    model->setItem(row, 2, itemOffset);

    QStandardItem* itemSize = new QStandardItem();
    QString size2 = QString::number(size, 16).toUpper();

    itemSize->setText(QString("0x%1").arg(size2));
    model->setItem(row, 3, itemSize);
}

void PDBExplorer::AddItemToModel(const QString& name, DWORD virtualOffset, DWORD fileOffset, QStandardItemModel* model, int row)
{
    QStandardItem* itemName = new QStandardItem();

    itemName->setText(name);
    model->setItem(row, 0, itemName);

    QStandardItem* itemVirtualOffset = new QStandardItem();
    QString virtualOffset2 = QString::number(virtualOffset, 16).toUpper();

    itemVirtualOffset->setText(QString("0x%1").arg(virtualOffset2));
    model->setItem(row, 1, itemVirtualOffset);

    QStandardItem* itemFileOffset = new QStandardItem();
    QString fileOffset2 = QString::number(fileOffset, 16).toUpper();

    itemFileOffset->setText(QString("0x%1").arg(fileOffset2));
    model->setItem(row, 2, itemFileOffset);
}

void PDBExplorer::AddItemToModel(const QString& functionPrototype, DWORD vTableIndex, QStandardItemModel* model, int row)
{
    QStandardItem* itemPrototype = new QStandardItem();

    itemPrototype->setText(functionPrototype);
    model->setItem(row, 0, itemPrototype);

    QStandardItem* itemIndex = new QStandardItem();

    itemIndex->setText(QString::number(vTableIndex));
    model->setItem(row, 1, itemIndex);

    QStandardItem* itemOffset = new QStandardItem();

    DWORD offset = vTableIndex << 2;
    QString offset2 = QString::number(offset, 16).toUpper();

    itemOffset->setText(QString("0x%1").arg(offset2));
    model->setItem(row, 2, itemOffset);
}

void PDBExplorer::SearchList()
{
    proxyModel->setFilterRegExp(txtFindItem->text());
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(0);
}

QString PDBExplorer::ConvertBOOLToString(BOOL state)
{
    if (state)
    {
        return "Yes";
    }

    return "No";
}

QString PDBExplorer::ConvertAddressToString(DWORD address)
{
    return QString("0x%1").arg(QString::number(address, 16).toUpper());
}

void PDBExplorer::CustomMenuRequested(QPoint position)
{
    menu->popup(ui.tvSymbols->viewport()->mapToGlobal(position));
}

void PDBExplorer::TxtFindItemTextChanged(const QString& text)
{
    SearchList();
}

void PDBExplorer::LVVTablesClicked(const QModelIndex& index)
{
    DisplayVTable(index.row());
}

void PDBExplorer::TxtMangledNameTextChanged(const QString& text)
{
    std::string mangledName = txtMangledName->text().toStdString();
    QString demangledName;

    if (options.useUndname)
    {
        demangledName = QString::fromStdString(peHeaderParser->UndecorateName(mangledName.c_str()));
    }
    else
    {
        std::string rest;

        demangledName = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
    }

    txtDemangledName->setText(demangledName);
}

void PDBExplorer::TxtMangledName2TextChanged(const QString& text)
{
    std::string mangledName = txtMangledName2->text().toStdString();
    QString demangledName;

    if (options.useUndname)
    {
        demangledName = QString::fromStdString(peHeaderParser->UndecorateName(mangledName.c_str()));
    }
    else
    {
        std::string rest;

        demangledName = QString::fromStdString(msvcDemangler.DemangleSymbol(mangledName, rest));
    }

    txtDemangledName2->setText(demangledName);
}

void PDBExplorer::TxtVirtualAddressTextChanged(const QString& text)
{
    if (!txtVirtualAddress->hasFocus())
    {
        if (txtRelativeVirtualAddress->hasFocus() && txtRelativeVirtualAddress->text().length() == 0)
        {
            txtVirtualAddress->clear();
        }
        else if (txtFileOffset->hasFocus() && txtFileOffset->text().length() == 0)
        {
            txtVirtualAddress->clear();
        }

        return;
    }

    if (txtVirtualAddress->text().length() == 0)
    {
        txtRelativeVirtualAddress->clear();
        txtFileOffset->clear();
    }
    else
    {
        bool status = false;
        DWORD virtualAddress = txtVirtualAddress->text().toULong(&status, 16);

        QString relativeVirtualAddress = ConvertAddressToString(peHeaderParser->ConvertVAToRVA(virtualAddress));
        QString fileOffset = ConvertAddressToString(peHeaderParser->ConvertVAToFileOffset(virtualAddress));

        txtRelativeVirtualAddress->setText(relativeVirtualAddress);
        txtFileOffset->setText(fileOffset);
    }
}

void PDBExplorer::TxtRelativeVirtualAddressTextChanged(const QString& text)
{
    if (!txtRelativeVirtualAddress->hasFocus())
    {
        if (txtVirtualAddress->hasFocus() && txtVirtualAddress->text().length() == 0)
        {
            txtRelativeVirtualAddress->clear();
        }
        else if (txtFileOffset->hasFocus() && txtFileOffset->text().length() == 0)
        {
            txtRelativeVirtualAddress->clear();
        }

        return;
    }

    if (txtRelativeVirtualAddress->text().length() == 0)
    {
        txtVirtualAddress->clear();
        txtFileOffset->clear();
    }
    else
    {
        bool status = false;
        DWORD relativeVirtualAddress = txtRelativeVirtualAddress->text().toULong(&status, 16);

        QString virtualAddress = ConvertAddressToString(peHeaderParser->ConvertRVAToVA(relativeVirtualAddress));
        QString fileOffset = ConvertAddressToString(peHeaderParser->ConvertRVAToFileOffset(relativeVirtualAddress));

        txtVirtualAddress->setText(virtualAddress);
        txtFileOffset->setText(fileOffset);
    }
}

void PDBExplorer::TxtFileOffsetTextChanged(const QString& text)
{
    if (!txtFileOffset->hasFocus())
    {
        if (txtVirtualAddress->hasFocus() && txtVirtualAddress->text().length() == 0)
        {
            txtFileOffset->clear();
        }
        else if (txtRelativeVirtualAddress->hasFocus() && txtRelativeVirtualAddress->text().length() == 0)
        {
            txtFileOffset->clear();
        }

        return;
    }

    if (txtFileOffset->text().length() == 0)
    {
        txtVirtualAddress->clear();
        txtRelativeVirtualAddress->clear();
    }
    else
    {
        bool status = false;
        DWORD fileOffset = txtFileOffset->text().toULong(&status, 16);

        QString virtualAddress = ConvertAddressToString(peHeaderParser->ConvertFileOffsetToVA(fileOffset));
        QString relativeVirtualAddress = ConvertAddressToString(peHeaderParser->ConvertFileOffsetToRVA(fileOffset));

        txtVirtualAddress->setText(virtualAddress);
        txtRelativeVirtualAddress->setText(relativeVirtualAddress);
    }
}

void PDBExplorer::ExportSymbol()
{
    SymbolRecord symbolRecord = GetSelectedSymbolRecord();

    if (symbolRecord.typeName.length() == 0)
    {
        return;
    }

    if (options.exportDependencies)
    {
        pdb->SetProcessType(ProcessType::exportUDTsAndEnumsWithDependencies);

        PDBProcessDialog pdbProcessDialog(this, pdb, ProcessType::exportUDTsAndEnumsWithDependencies, &symbolRecord);
        pdbProcessDialog.exec();
    }
    else
    {
        pdb->SetProcessType(ProcessType::exportUDTsAndEnums);
        pdb->ExportSymbol(symbolRecord);
    }
}
