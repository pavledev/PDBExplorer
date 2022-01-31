#include "OptionsDialog.h"

OptionsDialog::OptionsDialog(QWidget* parent, Options* options, bool* optionsChanged) : QDialog(parent)
{
	ui.setupUi(this);

    this->options = options;
    this->optionsChanged = optionsChanged;

    ui.chkDisplayIncludes->setChecked(options->displayIncludes);
    ui.chkDisplayComments->setChecked(options->displayComments);
    ui.chkDisplayNonImplementedFunctions->setChecked(options->displayNonImplementedFunctions);
    ui.chkDisplayVTablePointerIfExists->setChecked(options->displayVTablePointerIfExists);
    ui.chkDisplayTypedefs->setChecked(options->displayTypedefs);
    ui.chkDisplayFriendFunctions->setChecked(options->displayFriendFunctionsAndClasses);
    ui.chkDisplayEmptyUDTAndEnums->setChecked(options->displayEmptyUDTAndEnums);
    ui.chkDisplayCallingConventions->setChecked(options->displayCallingConventions);
    ui.chkDisplayCallingConventionsForFunctionPointers->setChecked(options->displayCallingConventionForFunctionPointers);
    ui.chkAddDefaultCtorAndDtorToUDT->setChecked(options->addDefaultCtorAndDtorToUDT);
    ui.chkApplyRuleOfThree->setChecked(options->applyRuleOfThree);
    ui.chkApplyReturnValueOptimization->setChecked(options->applyReturnValueOptimization);
    ui.chkApplyEmptyBaseClassOptimization->setChecked(options->applyEmptyBaseClassOptimization);
    ui.chkRemoveScopeResolutionOperator->setChecked(options->removeScopeResolutionOperator);
    ui.chkDeclareFunctionsForStaticVariables->setChecked(options->declareFunctionsForStaticVariables);
    ui.chkIncludeConstKeyword->setChecked(options->includeConstKeyword);
    ui.chkIncludeVolatileKeyword->setChecked(options->includeVolatileKeyword);
    ui.chkIncludeOnlyPublicAccessSpecifier->setChecked(options->includeOnlyPublicAccessSpecifier);
    ui.chkAddInlineKeywordToInlineFunctions->setChecked(options->addInlineKeywordToInlineFunctions);
    ui.chkDeclareStaticVariablesWithInlineKeyword->setChecked(options->declareStaticVariablesWithInlineKeyword);
    ui.chkAddDeclspecKeywords->setChecked(options->addDeclspecKeywords);
    ui.chkAddNoVTableKeyword->setChecked(options->addNoVTableKeyword);
    ui.chkAddExplicitKeyword->setChecked(options->addExplicitKeyword);
    ui.chkAddNoexceptKeyword->setChecked(options->addNoexceptKeyword);

    ui.chkSpecifyTypeAlignment->setChecked(options->specifyTypeAlignment);
    ui.chkDisplayPaddingBytes->setChecked(options->displayPaddingBytes);

    ui.rbGenerateOnlyHeader->setChecked(options->generateOnlyHeader);
    ui.rbGenerateOnlySource->setChecked(options->generateOnlySource);
    ui.rbGenerateBoth->setChecked(options->generateBoth);
    ui.chkExportDependencies->setChecked(options->exportDependencies);

    ui.rbTrailingReturnType->setChecked(options->displayWithTrailingReturnType);
    ui.rbTypedef->setChecked(options->displayWithTypedef);
    ui.rbUsing->setChecked(options->displayWithUsing);
    ui.rbAuto->setChecked(options->displayWithAuto);

    ui.rbUseTemplateFunction->setChecked(options->useTemplateFunction);
    ui.rbUseVAList->setChecked(options->useVAList);

    ui.rbUseTypedefKeyword->setChecked(options->useTypedefKeyword);
    ui.rbUseUsingKeyword->setChecked(options->useUsingKeyword);

    ui.chkImplementDefaultConstructorAndDestructor->setChecked(options->implementDefaultConstructorAndDestructor);
    ui.chkImplementMethodsOfInnerUDT->setChecked(options->implementMethodsOfInnerUDT);

    ui.rbUseUndname->setChecked(options->useUndname);
    ui.rbUseCustomDemangler->setChecked(options->useCustomDemangler);

    ui.rbIDANameStyle->setChecked(options->useIDANameStyle);
    ui.rbGhidraNameStyle->setChecked(options->useGhidraNameStyle);

    ui.chkRemoveHunNotFromUDTAndEnums->setChecked(options->removeHungaryNotationFromUDTAndEnums);

    ui.chkModifyFunctionNames->setChecked(options->modifyFunctionNames);

    if (!options->modifyFunctionNames)
    {
        ui.rbFunctionCamelCase->setEnabled(false);
        ui.rbFunctionPascalCase->setEnabled(false);
        ui.rbFunctionSnakeCase->setEnabled(false);
    }

    ui.rbFunctionCamelCase->setChecked(options->functionCamelCase);
    ui.rbFunctionPascalCase->setChecked(options->functionPascalCase);
    ui.rbFunctionSnakeCase->setChecked(options->functionSnakeCase);

    ui.chkModifyVariableNames->setChecked(options->modifyVariableNames);

    if (!options->modifyFunctionNames)
    {
        ui.rbVariableCamelCase->setEnabled(false);
        ui.rbVariablePascalCase->setEnabled(false);
        ui.rbVariableSnakeCase->setEnabled(false);
        ui.chkRemoveHunNotFromVariable->setEnabled(false);
    }

    ui.rbVariableCamelCase->setChecked(options->variableCamelCase);
    ui.rbVariablePascalCase->setChecked(options->variablePascalCase);
    ui.rbVariableSnakeCase->setChecked(options->variableSnakeCase);
    ui.chkRemoveHunNotFromVariable->setChecked(options->removeHungaryNotationFromVariable);

    connect(ui.btnOk, &QPushButton::clicked, this, &OptionsDialog::BtnOkClicked);
    connect(ui.btnCancel, &QPushButton::clicked, this, &OptionsDialog::BtnCancelClicked);
    connect(ui.chkModifyFunctionNames, &QCheckBox::clicked, this, &OptionsDialog::ChkModifyFunctionNamesClicked);
    connect(ui.chkModifyVariableNames, &QCheckBox::clicked, this, &OptionsDialog::ChkModifyVariableNamesClicked);
}

OptionsDialog::~OptionsDialog()
{
}

void OptionsDialog::BtnOkClicked()
{
    CheckIfOptionsChanged();

    options->displayIncludes = ui.chkDisplayIncludes->isChecked();
    options->displayComments = ui.chkDisplayComments->isChecked();
    options->displayNonImplementedFunctions = ui.chkDisplayNonImplementedFunctions->isChecked();
    options->displayVTablePointerIfExists = ui.chkDisplayVTablePointerIfExists->isChecked();
    options->displayTypedefs = ui.chkDisplayTypedefs->isChecked();
    options->displayFriendFunctionsAndClasses = ui.chkDisplayFriendFunctions->isChecked();
    options->displayEmptyUDTAndEnums = ui.chkDisplayEmptyUDTAndEnums->isChecked();
    options->displayCallingConventions = ui.chkDisplayCallingConventions->isChecked();
    options->displayCallingConventionForFunctionPointers = ui.chkDisplayCallingConventionsForFunctionPointers->isChecked();
    options->addDefaultCtorAndDtorToUDT = ui.chkAddDefaultCtorAndDtorToUDT->isChecked();
    options->applyRuleOfThree = ui.chkApplyRuleOfThree->isChecked();
    options->applyReturnValueOptimization = ui.chkApplyReturnValueOptimization->isChecked();
    options->applyEmptyBaseClassOptimization = ui.chkApplyEmptyBaseClassOptimization->isChecked();
    options->removeScopeResolutionOperator = ui.chkRemoveScopeResolutionOperator->isChecked();
    options->declareFunctionsForStaticVariables = ui.chkDeclareFunctionsForStaticVariables->isChecked();
    options->includeConstKeyword = ui.chkIncludeConstKeyword->isChecked();
    options->includeVolatileKeyword = ui.chkIncludeVolatileKeyword->isChecked();
    options->includeOnlyPublicAccessSpecifier = ui.chkIncludeOnlyPublicAccessSpecifier->isChecked();
    options->addInlineKeywordToInlineFunctions = ui.chkAddInlineKeywordToInlineFunctions->isChecked();
    options->declareStaticVariablesWithInlineKeyword = ui.chkDeclareStaticVariablesWithInlineKeyword->isChecked();
    options->addDeclspecKeywords = ui.chkAddDeclspecKeywords->isChecked();
    options->addNoVTableKeyword = ui.chkAddNoVTableKeyword->isChecked();
    options->addExplicitKeyword = ui.chkAddExplicitKeyword->isChecked();
    options->addNoexceptKeyword = ui.chkAddNoexceptKeyword->isChecked();

    options->specifyTypeAlignment = ui.chkSpecifyTypeAlignment->isChecked();
    options->displayPaddingBytes = ui.chkDisplayPaddingBytes->isChecked();

    options->generateOnlyHeader = ui.rbGenerateOnlyHeader->isChecked();
    options->generateOnlySource = ui.rbGenerateOnlySource->isChecked();
    options->generateBoth = ui.rbGenerateBoth->isChecked();
    options->exportDependencies = ui.chkExportDependencies->isChecked();

    options->displayWithTrailingReturnType = ui.rbTrailingReturnType->isChecked();
    options->displayWithTypedef = ui.rbTypedef->isChecked();
    options->displayWithUsing = ui.rbUsing->isChecked();
    options->displayWithAuto = ui.rbAuto->isChecked();

    options->useTemplateFunction = ui.rbUseTemplateFunction->isChecked();
    options->useVAList = ui.rbUseVAList->isChecked();

    options->useTypedefKeyword = ui.rbUseTypedefKeyword->isChecked();
    options->useUsingKeyword = ui.rbUseUsingKeyword->isChecked();

    options->implementDefaultConstructorAndDestructor = ui.chkImplementDefaultConstructorAndDestructor->isChecked();
    options->implementMethodsOfInnerUDT = ui.chkImplementMethodsOfInnerUDT->isChecked();

    options->useUndname = ui.rbUseUndname->isChecked();
    options->useCustomDemangler = ui.rbUseCustomDemangler->isChecked();

    options->useIDANameStyle = ui.rbIDANameStyle->isChecked();
    options->useGhidraNameStyle = ui.rbGhidraNameStyle->isChecked();

    options->removeHungaryNotationFromUDTAndEnums = ui.chkRemoveHunNotFromUDTAndEnums->isChecked();

    options->modifyFunctionNames = ui.chkModifyFunctionNames->isChecked();
    options->functionCamelCase = ui.rbFunctionCamelCase->isChecked();
    options->functionPascalCase = ui.rbFunctionPascalCase->isChecked();
    options->functionSnakeCase = ui.rbFunctionSnakeCase->isChecked();

    options->modifyVariableNames = ui.chkModifyVariableNames->isChecked();
    options->variableCamelCase = ui.rbVariableCamelCase->isChecked();
    options->variablePascalCase = ui.rbVariablePascalCase->isChecked();
    options->variableSnakeCase = ui.rbVariableSnakeCase->isChecked();
    options->removeHungaryNotationFromVariable = ui.chkRemoveHunNotFromVariable->isChecked();

    close();

    SaveOptions();
}

void OptionsDialog::BtnCancelClicked()
{
    close();
}

void OptionsDialog::ChkModifyFunctionNamesClicked()
{
    ui.rbFunctionCamelCase->setEnabled(ui.chkModifyFunctionNames->isChecked());
    ui.rbFunctionPascalCase->setEnabled(ui.chkModifyFunctionNames->isChecked());
    ui.rbFunctionSnakeCase->setEnabled(ui.chkModifyFunctionNames->isChecked());
}

void OptionsDialog::ChkModifyVariableNamesClicked()
{
    ui.rbVariableCamelCase->setEnabled(ui.chkModifyVariableNames->isChecked());
    ui.rbVariablePascalCase->setEnabled(ui.chkModifyVariableNames->isChecked());
    ui.rbVariableSnakeCase->setEnabled(ui.chkModifyVariableNames->isChecked());
    ui.chkRemoveHunNotFromVariable->setEnabled(ui.chkModifyVariableNames->isChecked());
}

void OptionsDialog::LoadOptions(Options* options)
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/PDBExplorer.ini", QSettings::IniFormat);

    options->lastDirectory = settings.value("LastDirectory").toString();

    options->displayIncludes = settings.value("DisplayIncludes").toBool();
    options->displayComments = settings.value("DisplayComments").toBool();
    options->displayNonImplementedFunctions = settings.value("DisplayNonImplementedFunctions").toBool();
    options->displayVTablePointerIfExists = settings.value("DisplayVTablePointerIfExists").toBool();
    options->displayTypedefs = settings.value("DisplayTypedefs").toBool();
    options->displayFriendFunctionsAndClasses = settings.value("DisplayFriendFunctionsAndClasses").toBool();
    options->displayEmptyUDTAndEnums = settings.value("DisplayEmptyUDTAndEnums").toBool();
    options->displayCallingConventions = settings.value("DisplayCallingConventions").toBool();
    options->displayCallingConventionForFunctionPointers = settings.value("DisplayCallingConventionForFunctionPointers").toBool();
    options->addDefaultCtorAndDtorToUDT = settings.value("AddDefaultCtorAndDtorToUDT").toBool();
    options->applyRuleOfThree = settings.value("ApplyRuleOfThree").toBool();
    options->applyReturnValueOptimization = settings.value("ApplyReturnValueOptimization").toBool();
    options->applyEmptyBaseClassOptimization = settings.value("ApplyEmptyBaseClassOptimization").toBool();
    options->removeScopeResolutionOperator = settings.value("RemoveScopeResolutionOperator").toBool();
    options->declareFunctionsForStaticVariables = settings.value("DeclareFunctionsForStaticVariables").toBool();
    options->includeConstKeyword = settings.value("IncludeConstKeyword").toBool();
    options->includeVolatileKeyword = settings.value("IncludeVolatileKeyword").toBool();
    options->includeOnlyPublicAccessSpecifier = settings.value("IncludeOnlyPublicAccessSpecifier").toBool();
    options->addInlineKeywordToInlineFunctions = settings.value("AddInlineKeywordToInlineFunctions").toBool();
    options->declareStaticVariablesWithInlineKeyword = settings.value("DeclareStaticVariablesWithInlineKeyword").toBool();
    options->addDeclspecKeywords = settings.value("AddDeclspecKeywords").toBool();
    options->addNoVTableKeyword = settings.value("AddNoVTableKeyword").toBool();
    options->addExplicitKeyword = settings.value("AddExplicitKeyword").toBool();
    options->addNoexceptKeyword = settings.value("AddNoexceptKeyword").toBool();

    options->specifyTypeAlignment = settings.value("SpecifyTypeAlignment").toBool();
    options->displayPaddingBytes = settings.value("DisplayPaddingBytes").toBool();

    options->generateOnlyHeader = settings.value("GenerateOnlyHeader").toBool();
    options->generateOnlySource = settings.value("GenerateOnlySource").toBool();
    options->generateBoth = settings.value("GenerateBoth").toBool();
    options->exportDependencies = settings.value("ExportDependencies").toBool();

    options->displayWithTrailingReturnType = settings.value("DisplayWithTrailingReturnType").toBool();
    options->displayWithTypedef = settings.value("DisplayWithTypedef").toBool();
    options->displayWithUsing = settings.value("DisplayWithUsing").toBool();
    options->displayWithAuto = settings.value("DisplayWithAuto").toBool();

    options->useTemplateFunction = settings.value("UseTemplateFunction").toBool();
    options->useVAList = settings.value("UseVAList").toBool();

    options->useTypedefKeyword = settings.value("UseTypedefKeyword").toBool();
    options->useUsingKeyword = settings.value("UseUsingKeyword").toBool();

    options->implementDefaultConstructorAndDestructor = settings.value("ImplementDefaultConstructorAndDestructor").toBool();
    options->implementMethodsOfInnerUDT = settings.value("ImplementMethodsOfInnerUDT").toBool();

    options->useUndname = settings.value("UseUndname").toBool();
    options->useCustomDemangler = settings.value("UseCustomDemangler").toBool();

    options->useIDANameStyle = settings.value("UseIDANameStyle").toBool();
    options->useGhidraNameStyle = settings.value("UseGhidraNameStyle").toBool();

    options->removeHungaryNotationFromUDTAndEnums = settings.value("RemoveHungaryNotationFromUDTAndEnums").toBool();

    options->modifyFunctionNames = settings.value("ModifyFunctionNames").toBool();
    options->functionCamelCase = settings.value("FunctionCamelCase").toBool();
    options->functionPascalCase = settings.value("FunctionPascalCase").toBool();
    options->functionSnakeCase = settings.value("FunctionSnakeCase").toBool();

    options->modifyVariableNames = settings.value("ModifyVariableNames").toBool();
    options->variableCamelCase = settings.value("VariableCamelCase").toBool();
    options->variablePascalCase = settings.value("VariablePascalCase").toBool();
    options->variableSnakeCase = settings.value("VariableSnakeCase").toBool();
    options->removeHungaryNotationFromVariable = settings.value("RemoveHungaryNotationFromVariable").toBool();
}

void OptionsDialog::SaveOptions()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/PDBExplorer.ini", QSettings::IniFormat);

    settings.setValue("DisplayIncludes", options->displayIncludes);
    settings.setValue("DisplayComments", options->displayComments);
    settings.setValue("DisplayNonImplementedFunctions", options->displayNonImplementedFunctions);
    settings.setValue("DisplayVTablePointerIfExists", options->displayVTablePointerIfExists);
    settings.setValue("DisplayTypedefs", options->displayTypedefs);
    settings.setValue("DisplayFriendFunctionsAndClasses", options->displayFriendFunctionsAndClasses);
    settings.setValue("DisplayEmptyUDTAndEnums", options->displayEmptyUDTAndEnums);
    settings.setValue("DisplayCallingConventions", options->displayCallingConventions);
    settings.setValue("DisplayCallingConventionForFunctionPointers", options->displayCallingConventionForFunctionPointers);
    settings.setValue("AddDefaultCtorAndDtorToUDT", options->addDefaultCtorAndDtorToUDT);
    settings.setValue("ApplyRuleOfThree", options->applyRuleOfThree);
    settings.setValue("ApplyReturnValueOptimization", options->applyReturnValueOptimization);
    settings.setValue("ApplyEmptyBaseClassOptimization", options->applyEmptyBaseClassOptimization);
    settings.setValue("RemoveScopeResolutionOperator", options->removeScopeResolutionOperator);
    settings.setValue("DeclareFunctionsForStaticVariables", options->declareFunctionsForStaticVariables);
    settings.setValue("IncludeConstKeyword", options->includeConstKeyword);
    settings.setValue("IncludeVolatileKeyword", options->includeVolatileKeyword);
    settings.setValue("IncludeOnlyPublicAccessSpecifier", options->includeOnlyPublicAccessSpecifier);
    settings.setValue("AddInlineKeywordToInlineFunctions", options->addInlineKeywordToInlineFunctions);
    settings.setValue("DeclareStaticVariablesWithInlineKeyword", options->declareStaticVariablesWithInlineKeyword);
    settings.setValue("AddDeclspecKeywords", options->addDeclspecKeywords);
    settings.setValue("AddNoVTableKeyword", options->addNoVTableKeyword);
    settings.setValue("AddExplicitKeyword", options->addExplicitKeyword);
    settings.setValue("AddNoexceptKeyword", options->addNoexceptKeyword);

    settings.setValue("SpecifyTypeAlignment", options->specifyTypeAlignment);
    settings.setValue("DisplayPaddingBytes", options->displayPaddingBytes);

    settings.setValue("GenerateOnlyHeader", options->generateOnlyHeader);
    settings.setValue("GenerateOnlySource", options->generateOnlySource);
    settings.setValue("GenerateBoth", options->generateBoth);
    settings.setValue("ExportDependencies", options->exportDependencies);

    settings.setValue("DisplayWithTrailingReturnType", options->displayWithTrailingReturnType);
    settings.setValue("DisplayWithTypedef", options->displayWithTypedef);
    settings.setValue("DisplayWithUsing", options->displayWithUsing);
    settings.setValue("DisplayWithAuto", options->displayWithAuto);

    settings.setValue("UseTemplateFunction", options->useTemplateFunction);
    settings.setValue("UseVAList", options->useVAList);

    settings.setValue("UseTypedefKeyword", options->useTypedefKeyword);
    settings.setValue("UseUsingKeyword", options->useUsingKeyword);

    settings.setValue("ImplementDefaultConstructorAndDestructor", options->implementDefaultConstructorAndDestructor);
    settings.setValue("ImplementMethodsOfInnerUDT", options->implementMethodsOfInnerUDT);

    settings.setValue("UseUndname", options->useUndname);
    settings.setValue("UseCustomDemangler", options->useCustomDemangler);

    settings.setValue("UseIDANameStyle", options->useIDANameStyle);
    settings.setValue("UseGhidraNameStyle", options->useGhidraNameStyle);

    settings.setValue("RemoveHungaryNotationFromUDTAndEnums", options->removeHungaryNotationFromUDTAndEnums);

    settings.setValue("ModifyFunctionNames", options->modifyFunctionNames);
    settings.setValue("FunctionCamelCase", options->functionCamelCase);
    settings.setValue("FunctionPascalCase", options->functionPascalCase);
    settings.setValue("FunctionSnakeCase", options->functionSnakeCase);

    settings.setValue("ModifyVariableNames", options->modifyVariableNames);
    settings.setValue("VariableCamelCase", options->variableCamelCase);
    settings.setValue("VariablePascalCase", options->variablePascalCase);
    settings.setValue("VariableSnakeCase", options->variableSnakeCase);
    settings.setValue("RemoveHungaryNotationFromVariable", options->removeHungaryNotationFromVariable);
}

void OptionsDialog::CheckIfOptionsChanged()
{
    if (options->displayIncludes != ui.chkDisplayIncludes->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayComments != ui.chkDisplayComments->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayNonImplementedFunctions != ui.chkDisplayNonImplementedFunctions->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayVTablePointerIfExists != ui.chkDisplayVTablePointerIfExists->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayTypedefs != ui.chkDisplayTypedefs->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayFriendFunctionsAndClasses != ui.chkDisplayFriendFunctions->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayEmptyUDTAndEnums != ui.chkDisplayEmptyUDTAndEnums->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayCallingConventions != ui.chkDisplayCallingConventions->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayCallingConventionForFunctionPointers !=
        ui.chkDisplayCallingConventionsForFunctionPointers->isChecked() &&
        !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addDefaultCtorAndDtorToUDT != ui.chkAddDefaultCtorAndDtorToUDT->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->applyRuleOfThree != ui.chkApplyRuleOfThree->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->applyReturnValueOptimization != ui.chkApplyReturnValueOptimization->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->applyEmptyBaseClassOptimization != ui.chkApplyEmptyBaseClassOptimization->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->removeScopeResolutionOperator != ui.chkRemoveScopeResolutionOperator->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->declareFunctionsForStaticVariables != ui.chkDeclareFunctionsForStaticVariables->isChecked() &&
        !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->includeConstKeyword != ui.chkIncludeConstKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->includeVolatileKeyword != ui.chkIncludeVolatileKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->includeOnlyPublicAccessSpecifier != ui.chkIncludeOnlyPublicAccessSpecifier->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addInlineKeywordToInlineFunctions != ui.chkAddInlineKeywordToInlineFunctions->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->declareStaticVariablesWithInlineKeyword != ui.chkDeclareStaticVariablesWithInlineKeyword->isChecked() &&
        !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addDeclspecKeywords != ui.chkAddDeclspecKeywords->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addNoVTableKeyword != ui.chkAddNoVTableKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addExplicitKeyword != ui.chkAddExplicitKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->addNoexceptKeyword != ui.chkAddNoexceptKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->specifyTypeAlignment != ui.chkSpecifyTypeAlignment->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayPaddingBytes != ui.chkDisplayPaddingBytes->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->generateOnlyHeader != ui.rbGenerateOnlyHeader->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->generateOnlySource != ui.rbGenerateOnlySource->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->generateBoth != ui.rbGenerateBoth->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->exportDependencies != ui.chkExportDependencies->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayWithTrailingReturnType != ui.rbTrailingReturnType->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayWithTypedef != ui.rbTypedef->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayWithUsing != ui.rbUsing->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->displayWithAuto != ui.rbAuto->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useTemplateFunction != ui.rbUseTemplateFunction->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useVAList != ui.rbUseVAList->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useTypedefKeyword != ui.rbUseTypedefKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useUsingKeyword != ui.rbUseUsingKeyword->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useUndname != ui.rbUseUndname->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useCustomDemangler != ui.rbUseCustomDemangler->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useIDANameStyle != ui.rbIDANameStyle->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->useGhidraNameStyle != ui.rbGhidraNameStyle->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->removeHungaryNotationFromUDTAndEnums != ui.chkRemoveHunNotFromUDTAndEnums->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->modifyFunctionNames != ui.chkModifyFunctionNames->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->functionCamelCase != ui.rbFunctionCamelCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->functionPascalCase != ui.rbFunctionPascalCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->functionSnakeCase != ui.rbFunctionSnakeCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->modifyVariableNames != ui.chkModifyVariableNames->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->variableCamelCase != ui.rbVariableCamelCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->variablePascalCase != ui.rbVariablePascalCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->variableSnakeCase != ui.rbVariableSnakeCase->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }

    if (options->removeHungaryNotationFromVariable != ui.chkRemoveHunNotFromVariable->isChecked() && !*optionsChanged)
    {
        *optionsChanged = true;
    }
}
