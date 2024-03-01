// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketIO_MMO.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "sio_client.h"

#define LOCTEXT_NAMESPACE "FSocketIO_MMOModule"

void FSocketIO_MMOModule::StartupModule()
{
	return;
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Get the base directory of this plugin
	//FString BaseDir = IPluginManager::Get().FindPlugin("SocketIO_MMO")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	//FString LibraryPath;
#if PLATFORM_WINDOWS
	//LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SocketIO_MMOLibrary/Win64/ExampleLibrary.dll"));
#elif PLATFORM_MAC
    //LibraryPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/SocketIO_MMOLibrary/Mac/Release/libExampleLibrary.dylib"));
#elif PLATFORM_LINUX
	//LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/ThirdParty/SocketIO_MMOLibrary/Linux/x86_64-unknown-linux-gnu/libExampleLibrary.so"));
#endif // PLATFORM_WINDOWS

	//SockeIOLibraryHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

	/*
	if (ExampleLibraryHandle)
	{
		// Call the test function in the third party library that opens a message box
		ExampleLibraryFunction();
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ThirdPartyLibraryError", "Failed to load example third party library"));
	}
	*/
}

void FSocketIO_MMOModule::ShutdownModule()
{
	return;
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Free the dll handle
	//FPlatformProcess::FreeDllHandle(ExampleLibraryHandle);
	//ExampleLibraryHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSocketIO_MMOModule, SocketIO_MMO)
