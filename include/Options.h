#pragma once

#include <QString>

struct Options
{
	QString lastDirectory;
	bool displayIncludes = false;
	bool displayComments = false;
	bool displayNonImplementedFunctions = false;
	bool displayVTablePointerIfExists = false;
	bool displayTypedefs = false;
	bool displayFriendFunctionsAndClasses = false;
	bool displayEmptyUDTAndEnums = false;
	bool displayCallingConventions = false;
	bool displayCallingConventionForFunctionPointers = false;
	bool addDefaultCtorAndDtorToUDT = false;
	bool applyRuleOfThree = false;
	bool applyReturnValueOptimization = false;
	bool applyEmptyBaseClassOptimization = false;
	bool removeScopeResolutionOperator = false;
	bool declareFunctionsForStaticVariables = false;
	bool includeConstKeyword = false;
	bool includeVolatileKeyword = false;
	bool includeOnlyPublicAccessSpecifier = false;
	bool addInlineKeywordToInlineFunctions = false;
	bool declareStaticVariablesWithInlineKeyword = false;
	bool addDeclspecKeywords = false;
	bool addNoVTableKeyword = false;
	bool addExplicitKeyword = false;
	bool addNoexceptKeyword = false;
	
	bool specifyTypeAlignment = false;
	bool displayPaddingBytes = false;

	bool generateOnlyHeader = false;
	bool generateOnlySource = false;
	bool generateBoth = false;
	bool exportDependencies = false;

	bool displayWithTrailingReturnType = false;
	bool displayWithTypedef = false;
	bool displayWithUsing = false;
	bool displayWithAuto = false;

	bool useTemplateFunction = false;
	bool useVAList = false;

	bool useTypedefKeyword = false;
	bool useUsingKeyword = false;

	bool implementDefaultConstructorAndDestructor = false;
	bool implementMethodsOfInnerUDT = false;

	bool useUndname;
	bool useCustomDemangler;

	bool useIDANameStyle = false;
	bool useGhidraNameStyle = false;

	bool removeHungaryNotationFromUDTAndEnums = false;

	bool modifyFunctionNames = false;
	bool functionCamelCase = false;
	bool functionPascalCase = false;
	bool functionSnakeCase = false;

	bool modifyVariableNames = false;
	bool variableCamelCase = false;
	bool variablePascalCase = false;
	bool variableSnakeCase = false;
	bool removeHungaryNotationFromVariable = false;
};
