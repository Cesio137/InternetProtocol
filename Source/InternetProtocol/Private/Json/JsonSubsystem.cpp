// Fill out your copyright notice in the Description page of Project Settings.


#include "Json/JsonSubsystem.h"

UJavaScriptObjectNotation* UJsonSubsystem::CreateJsonObject()
{
	UJavaScriptObjectNotation* JsonObject = NewObject<UJavaScriptObjectNotation>();
	return JsonObject;
}
