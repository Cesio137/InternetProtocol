//© Nathan Miguel, 2024. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FInternetProtocolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handle to the test dll we will load */
	/*
	void*	LegacyLibraryHandle;
	void*	CryptoLibraryHandle;
	void*	SslLibraryHandle;
	*/
};
