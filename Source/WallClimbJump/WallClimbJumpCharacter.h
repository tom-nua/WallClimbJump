// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
// #include "ClimbableWall.h"
#include "UIWidget.h"
#include "GameFramework/Character.h"
#include "WallClimbJumpCharacter.generated.h"

UCLASS(config=Game)
class AWallClimbJumpCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	UPROPERTY()
	class UCharAnimInstance* animController;
public:
	AWallClimbJumpCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=UI)
	TSubclassOf<UUIWidget> promptWidgetClass;

	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Animation)
	// TSubclassOf<UAnimSequence> climbAnim;
	
	void ShowPrompt(class AClimbableWall* newWall);
	void HidePrompt();
	void Detach();

protected:

	UPROPERTY(BlueprintReadOnly, Category="Widgets")
	UUIWidget* promptWidget;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	AClimbableWall* selectedWall;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	class ALedge* selectedLedge;

	bool bIsClimbing;
	bool bIsHoldingLedge;
	bool bIsRotating;
	FRotator RotateTarget;
	
	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

	void WallAttach();

	virtual void BeginPlay() override;
	virtual void Jump() override;
	virtual void StopJumping() override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

