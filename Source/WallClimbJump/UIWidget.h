// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UIWidget.generated.h"

/**
 * 
 */
UCLASS()
class WALLCLIMBJUMP_API UUIWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	UFUNCTION(BlueprintNativeEvent)
	void ShowPrompt(const FText &labelText);
	void ShowPrompt_Implementation(const FText &labelText);
	UFUNCTION(BlueprintNativeEvent)
	void HidePrompt();
	void HidePrompt_Implementation();
};
