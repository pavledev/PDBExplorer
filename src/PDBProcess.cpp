#include "PDBProcess.h"

PDBProcess::PDBProcess(QObject* parent, PDB* pdb, ProcessType processType, SymbolRecord* symbolRecord)
	: QObject(parent)
{
	this->pdb = pdb;
	this->processType = processType;
	this->symbolRecord = symbolRecord;

	connect(pdb, &PDB::Completed, this, &PDBProcess::Completed);
	connect(pdb, &PDB::SetProgressMinimum, this, &PDBProcess::SetProgressMinimum);
	connect(pdb, &PDB::SetProgressMaximum, this, &PDBProcess::SetProgressMaximum);
	connect(pdb, &PDB::SetProgressValue, this, &PDBProcess::SetProgressValue);

	connect(pdb, &PDB::SendStatusMessageToProcessDialog, this, &PDBProcess::SendStatusMessageToProcessDialog);
}

PDBProcess::~PDBProcess()
{

}

void PDBProcess::Process()
{
	switch (processType)
	{
	case ProcessType::importUDTsAndEnums:
		pdb->LoadPDBData();
		break;
	case ProcessType::importVariables:
		pdb->GetVariables();
		break;
	case ProcessType::importFunctions:
		pdb->GetFunctions();
		break;
	case ProcessType::importPublicSymbols:
		pdb->GetPublicSymbols();
		break;
	case ProcessType::exportUDTsAndEnumsWithDependencies:
		pdb->ExportSymbolWithDependencies(symbolRecord);
		break;
	case ProcessType::exportAllUDTsAndEnums:
		pdb->ExportAllSymbols();
		break;
	}
}

void PDBProcess::Stop()
{
	pdb->Stop();
}
