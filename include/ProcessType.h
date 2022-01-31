#pragma once

enum class ProcessType
{
	importUDTsAndEnums,
	importVariables,
	importFunctions,
	importPublicSymbols,
	exportUDTsAndEnums,
	exportUDTsAndEnumsWithDependencies,
	exportAllUDTsAndEnums
};
