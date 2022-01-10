// Fill out your copyright notice in the Description page of Project Settings.


#include "UIWidget.h"

void UUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Warning, TEXT("Running widget"))
}

void UUIWidget::ShowPrompt_Implementation(const FText &labelText)
{
	
}

void UUIWidget::HidePrompt_Implementation()
{
	
}