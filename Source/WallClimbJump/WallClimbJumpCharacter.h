// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
	class UCharAnimInstance* AnimController;
	
	UPROPERTY()
	class AGrappleTarget* TargetActor;
	
public:
	AWallClimbJumpCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=UI)
	TSubclassOf<class UUIWidget> PromptWidgetClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Gameplay)
	TArray<class ALedge*> Ledges;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=UI)
	TSubclassOf<AGrappleTarget> TargetActorClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Gameplay)
	class UCableComponent* CableComponent;
	
	void WallDetected(class AClimbableWall* NewWall);
	void WallUndetected();
	void ShowPrompt(FString NewText);
	void HidePrompt(FString NewText);
	void Detach();
	void StartGrapple();
	void Grapple();
	void GrappleTravel(float DeltaTime);
	void GrabLedge(const FVector HangLocation);
	void LocateTarget();

protected:

	UPROPERTY(BlueprintReadOnly, Category="Widgets")
	UUIWidget* PromptWidget;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	AClimbableWall* SelectedWall;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	ALedge* SelectedLedge;
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	ALedge* TargetLedge;

	UPROPERTY(BlueprintReadOnly, Category="Movement")
	ALedge* CurrentLedge;
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	ALedge* RightLedge;
	UPROPERTY(BlueprintReadOnly, Category="Movement")
	ALedge* LeftLedge;

	bool bIsClimbing;
	bool bIsHoldingLedge;
	bool bIsRotating;
	bool bIsGrapplePreparing;
	bool bIsGrappling;
	FVector GrapplePoint;
	FVector GrappleNormal;
	FVector HoldOffset;
	FVector RotateNormal;
	FVector LeftWallNormal;
	FVector RightWallNormal;
	FVector CableLocalPosition;
	float MoveDirection;
	FString CurrentPrompt;
	FTimerHandle GrappleTimerH;
	FCollisionShape CapsuleCollisionShape = FCollisionShape::MakeCapsule(14, 70);
	
	/** Resets HMD orientation in VR. */
	// void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	// /** 
	//  * Called via input to turn at a given rate. 
	//  * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	//  */
	// void TurnAtRate(float Rate);
	//
	// /**
	//  * Called via input to turn look up/down at a given rate. 
	//  * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	//  */
	// void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	// void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);
	
	/** Handler for when a touch input stops. */
	// void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

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
	/** Returns CameraBoom sub object **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera sub object **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

