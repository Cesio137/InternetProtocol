/** 
 * Copyright © 2025 Nathan Miguel
 */

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
};
