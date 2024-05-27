// Fill out your copyright notice in the Description page of Project Settings.


#include "HTTP/HttpClientObject.h"

UHttpClientObject::UHttpClientObject()
	: request(NewObject<URequestObject>())
{
}
