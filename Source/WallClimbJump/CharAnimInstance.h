// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CharAnimInstance.generated.h"

/**
 * 
 */
UENUM()
enum class EJumpDirection : uint8 { Left, Right };

UCLASS()
class WALLCLIMBJUMP_API UCharAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	bool bIsClimbing;
	UPROPERTY(BlueprintReadOnly)
	float Direction;
	UPROPERTY(BlueprintReadOnly)
	bool bIsHolding;
	UPROPERTY(BlueprintReadOnly)
	EJumpDirection JumpDirection;
	UPROPERTY(BlueprintReadOnly)
	bool IsJumpingOff;
	
};
