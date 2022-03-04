// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallClimbJumpCharacter.h"

#include "CharAnimInstance.h"
#include "ClimbableWall.h"
#include "DrawDebugHelpers.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Ledge.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// AWallClimbJumpCharacter

AWallClimbJumpCharacter::AWallClimbJumpCharacter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

}

void AWallClimbJumpCharacter::BeginPlay()
{
	Super::BeginPlay();
	if(promptWidgetClass)
	{
		UUserWidget* userWidget = CreateWidget(GetWorld()->GetFirstPlayerController(), promptWidgetClass);
		if(!userWidget){return;}
		userWidget->AddToViewport(0);
		promptWidget = Cast<UUIWidget>(userWidget);		
	}
	UAnimInstance* animInstance = GetMesh()->GetAnimInstance();
	if(animInstance)
	{
		animController = Cast<UCharAnimInstance>(animInstance);
	}
}

void AWallClimbJumpCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(bIsClimbing)
	{
		if(GetVelocity().IsZero())
		{
			GetMesh()->GlobalAnimRateScale = 0.0f;
		}
		else
		{
			GetMesh()->GlobalAnimRateScale = 1.0f;
		}
		// FString v = GetVelocity().ToString();
		// UE_LOG(LogTemp, Warning, TEXT("%s"), *v)
	}
	if(currentLedge && bIsRotating && bIsHoldingLedge)
	{
		// const float ForwardDotProduct = FVector::DotProduct(GetActorForwardVector(), selectedLedge->GetActorRightVector());
		// const float RightDotProduct = FVector::DotProduct(GetActorRightVector(), selectedLedge->GetActorForwardVector());
		const float ForwardDotProduct = FVector::DotProduct(currentLedge->GetActorForwardVector(), GetActorForwardVector());
		// const float RightDotProduct = FVector::DotProduct(currentLedge->GetActorRightVector(), GetActorRightVector());
		UE_LOG(LogTemp, Warning, TEXT("Forward:%f"), ForwardDotProduct);
		if(ForwardDotProduct < 0)
		{
			if(ForwardDotProduct > 0.010000)
			{
				bIsRotating = false;
				return;
			}
			FRotator currentRot = GetActorRotation();
			SetActorRotation(currentRot.Add(0, -1, 0));
		}else if(ForwardDotProduct > 0)
		{
			if(ForwardDotProduct < 0.010000)
			{
				bIsRotating = false;
				return;
			}
			FRotator currentRot = GetActorRotation();
			SetActorRotation(currentRot.Add(0, 1, 0));
		}
		// FMath::RInterpTo(GetActorRotation(), RotateTarget, DeltaTime, 1);
	}
	FHitResult ledgeOutHit;
	FHitResult wallOutHit;
	FCollisionQueryParams collisionParams;
	collisionParams.AddIgnoredActor(this);
	FVector actorLoc = GetActorLocation();
	DrawDebugLine(GetWorld(), actorLoc, actorLoc + GetActorForwardVector() * 50, FColor::Green, false, 0.5, 0, 3);
	FVector startPos = actorLoc + GetActorForwardVector() * 40;
	FVector endPos = startPos + GetActorUpVector() * 140;
	DrawDebugLine(GetWorld(), startPos, endPos, FColor::Red, false, 0.5, 0, 3);
	if(GetWorld()->LineTraceSingleByChannel(ledgeOutHit, startPos, endPos, ECC_GameTraceChannel1, collisionParams))
	{
		ALedge* HitLedge = Cast<ALedge>(ledgeOutHit.Actor);
		if(HitLedge)
		{
			selectedLedge = HitLedge;
		}else
		{
			selectedLedge = nullptr;
		}
	}else
	{
		selectedLedge = nullptr;
	}
	if(GetWorld()->LineTraceSingleByChannel(wallOutHit, actorLoc, actorLoc + GetActorForwardVector() * 50, ECC_WorldStatic, collisionParams))
	{
		AClimbableWall* HitWall = Cast<AClimbableWall>(wallOutHit.Actor);
		if(HitWall == selectedWall)
		{
			return;
		}
		if(HitWall)
		{
			ShowPrompt(HitWall);
			// UE_LOG(LogTemp, Warning, TEXT("Hit"))
			return;
		}
	}
	HidePrompt();
}

//////////////////////////////////////////////////////////////////////////
// Input

void AWallClimbJumpCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AWallClimbJumpCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AWallClimbJumpCharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AWallClimbJumpCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AWallClimbJumpCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AWallClimbJumpCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AWallClimbJumpCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AWallClimbJumpCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AWallClimbJumpCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWallClimbJumpCharacter::OnResetVR);
	
	PlayerInputComponent->BindAction("Climb", IE_Pressed, this, &AWallClimbJumpCharacter::WallAttach);
}

void AWallClimbJumpCharacter::Detach()
{
	if(animController)
	{
		animController->bIsClimbing = false;
	}
	GetMesh()->GlobalAnimRateScale = 1.0f;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsClimbing = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AWallClimbJumpCharacter::WallAttach()
{
	if(bIsClimbing)
	{
		promptWidget->ShowPrompt(FText::FromString("E - Attach"));
		Detach();
	}
	else if(selectedWall)
	{
		if(animController)
		{
			animController->bIsClimbing = true;
		}
		bIsClimbing = true;
		GetCharacterMovement()->MaxFlySpeed = 100;
		GetCharacterMovement()->BrakingDecelerationFlying = 80;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately();
		
		promptWidget->ShowPrompt(FText::FromString("E - Detach"));
	}
}

void AWallClimbJumpCharacter::ShowPrompt(AClimbableWall* newWall)
{
	selectedWall = newWall;
	// UE_LOG(LogTemp, Warning, TEXT("Attempting to ShowPrompt"))
	if(!promptWidget || bIsClimbing)
	{
		return;
	}
	// UE_LOG(LogTemp, Warning, TEXT("Firing BP event"))
	promptWidget->ShowPrompt(FText::FromString("E - Climb"));
}

void AWallClimbJumpCharacter::HidePrompt()
{
	// if (wall != selectedWall)
        	// {
        	// 	return;
        	// }
	if(!selectedWall)
	{
		return;
	}
	selectedWall = nullptr;
	if(bIsClimbing)
	{
		Detach();
	}
	if(!promptWidget)
	{
		return;
	}
	// UE_LOG(LogTemp, Warning, TEXT("Hiding ui"))
	promptWidget->HidePrompt();
}

void AWallClimbJumpCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AWallClimbJumpCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AWallClimbJumpCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AWallClimbJumpCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AWallClimbJumpCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AWallClimbJumpCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		if(bIsClimbing)
		{
			GetMesh()->GlobalAnimRateScale = 1.0f;
			SetActorRotation(selectedWall->GetActorRotation(), ETeleportType::None);
			AddMovementInput(GetActorUpVector(), Value, false);
		}
		else if(bIsHoldingLedge)
		{
			
		}
		else
		{
			// get forward vector
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			AddMovementInput(Direction, Value);
		}
	}
}

void AWallClimbJumpCharacter::Jump()
{
	if(currentLedge)
	{
		UE_LOG(LogTemp, Warning, TEXT("current ledge"));
		bIsHoldingLedge = false;
		bIsRotating = false;
		currentLedge = nullptr;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		if(animController)
		{
			animController->bIsHolding = false;
		}
		if(!rightLedge && moveDirection > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("right ledge"));
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * 970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			// currentLedge = nullptr;
			return;
		}
		if(!leftLedge && moveDirection < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("left ledge"));
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * -970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			// currentLedge = nullptr;
			return;
		}
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return;
	}
	if(selectedLedge)
	{
		bIsHoldingLedge = true;
		bIsRotating = true;
		currentLedge = selectedLedge;
		if(animController)
		{
			animController->bIsHolding = true;
		}
		FVector hangLocation = GetMesh()->GetSocketLocation("hand_Socket");
		hangLocation.Z -= GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		SetActorLocation(hangLocation);
		GetCharacterMovement()->MaxFlySpeed = 50;
		GetCharacterMovement()->BrakingDecelerationFlying = 110;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	if(!bIsClimbing)
	{
		Super::Jump();
	}
}

void AWallClimbJumpCharacter::StopJumping()
{
	if(bIsClimbing || bIsHoldingLedge)
	{
		return;
	}
	Super::StopJumping();
}

void AWallClimbJumpCharacter::MoveRight(float Value)
{
	if(bIsHoldingLedge)
	{
		moveDirection = Value;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);
		// CollisionParams.AddIgnoredActor(currentLedge);
		FHitResult rightOutHit;
		FHitResult leftOutHit;
		FVector rightStartPos = GetActorLocation() + (GetActorRightVector() * 30) + (GetActorUpVector() * 120);
		FVector rightEndPos = rightStartPos + GetActorForwardVector() * 40;
		FVector leftStartPos = GetActorLocation() + (GetActorRightVector() * -30) + (GetActorUpVector() * 120);
		FVector leftEndPos = leftStartPos + GetActorForwardVector() * 40;
		DrawDebugLine(GetWorld(), rightStartPos, rightEndPos, FColor::Blue, false, 0.5, 0, 2);
		DrawDebugLine(GetWorld(), leftStartPos, leftEndPos, FColor::Green, false, 0.5, 0, 2);
		// if(Value == 0)
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("Moving Nowhere"))
		// 	rightLedge = nullptr;
		// 	leftLedge = nullptr;
		// 	if(animController)
		// 	{
		// 		animController->Direction = 0;
		// 	}
		// }
		if(Value > 0)
		{
			bool hitRight = GetWorld()->LineTraceSingleByChannel(rightOutHit, rightStartPos, rightEndPos, ECC_GameTraceChannel1, CollisionParams);
			if (hitRight)
			{
				// UE_LOG(LogTemp, Warning, TEXT("Moving Right"))
				ALedge* HitLedge = Cast<ALedge>(rightOutHit.Actor);
				if(HitLedge)
				{
					rightLedge = HitLedge;
					if(animController)
					{
						animController->Direction = Value;
						AddMovementInput(GetActorRightVector(), Value, false);
					}
				}
				else
				{
					rightLedge = nullptr;
					if(animController)
					{
						animController->Direction = 0;
					}
				}
			}
			else
			{
				rightLedge = nullptr;
				if(animController)
				{
					animController->Direction = 0;
				}
			}
		}
		else if(Value < 0)
		{
			bool hitLeft = GetWorld()->LineTraceSingleByChannel(leftOutHit, leftStartPos, leftEndPos, ECC_GameTraceChannel1, CollisionParams);
			if (hitLeft)
			{
				// UE_LOG(LogTemp, Warning, TEXT("Moving Right"))
				ALedge* HitLedge = Cast<ALedge>(leftOutHit.Actor);
				if(HitLedge)
				{
					leftLedge = HitLedge;
					if(animController)
					{
						animController->Direction = Value;
						AddMovementInput(GetActorRightVector(), Value, false);
					}
				}
				else
				{
					leftLedge = nullptr;
					if(animController)
					{
						animController->Direction = 0;
					}
				}
			}
			else
			{
				leftLedge = nullptr;
				if(animController)
				{
					animController->Direction = 0;
				}
			}
		}
		else
		{
			leftLedge = nullptr;
			rightLedge = nullptr;
			if(animController)
			{
				animController->Direction = 0;
			}
		}
		return;
	}
	if (!Controller || Value == 0.0f)
	{
		return;
	}
	if(bIsClimbing)
	{
		SetActorRotation(selectedWall->GetActorRotation(), ETeleportType::None);
		AddMovementInput(GetActorRightVector(), Value, false);
	}
	else
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
