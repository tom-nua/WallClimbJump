// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallClimbJumpCharacter.h"
#include "CharAnimInstance.h"
#include "ClimbableWall.h"
#include "DrawDebugHelpers.h"
#include "GrappleTarget.h"
#include "Ledge.h"
#include "UIWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"

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
	if(PromptWidgetClass)
	{
		UUserWidget* userWidget = CreateWidget(GetWorld()->GetFirstPlayerController(), PromptWidgetClass);
		if(!userWidget){return;}
		userWidget->AddToViewport(0);
		PromptWidget = Cast<UUIWidget>(userWidget);		
	}
	UAnimInstance* animInstance = GetMesh()->GetAnimInstance();
	if(animInstance)
	{
		AnimController = Cast<UCharAnimInstance>(animInstance);
	}
	if(TargetActorClass)
	{
		TargetActor = Cast<AGrappleTarget>(GetWorld()->SpawnActor(TargetActorClass));
	}
}

void AWallClimbJumpCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsGrappling)
	{
		GrappleTravel(DeltaTime);
	}
	else
	{
		LocateTarget();
	}
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
	}
	if(!TargetActor) return;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	CollisionParams.AddIgnoredActor(TargetActor);
	FVector ActorLoc = GetActorLocation();
	DrawDebugLine(GetWorld(), ActorLoc, ActorLoc + GetActorForwardVector() * 50, FColor::Green, false, 0.5, 0, 3);
	FVector StartPos = ActorLoc + GetActorForwardVector() * 40;
	FVector EndPos = StartPos + GetActorUpVector() * 140;
	DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Red, false, 0.5, 0, 3);

	if(bIsRotating && (bIsHoldingLedge && CurrentLedge || bIsClimbing || bIsGrapplePreparing))
	{
		const float ForwardDotProduct = FVector::DotProduct(WallTraceInfo.ImpactNormal, GetActorRightVector());
		UE_LOG(LogTemp, Warning, TEXT("Forward:%f"), ForwardDotProduct);
		if(ForwardDotProduct < 0)
		{
			if(ForwardDotProduct > -0.020000)
			{
				bIsRotating = false;
				return;
			}
			FRotator CurrentRot = GetActorRotation();
			SetActorRotation(CurrentRot.Add(0, 1, 0));
		}else if(ForwardDotProduct > 0)
		{
			if(ForwardDotProduct < 0.020000)
			{
				bIsRotating = false;
				return;
			}
			FRotator currentRot = GetActorRotation();
			SetActorRotation(currentRot.Add(0, -1, 0));
		}
	}
	if(!bIsHoldingLedge)
	{
		FHitResult LedgeOutHit;
		if(GetWorld()->LineTraceSingleByChannel(LedgeOutHit, StartPos, EndPos, ECC_GameTraceChannel1, CollisionParams))
		{
			ALedge* HitLedge = Cast<ALedge>(LedgeOutHit.Actor);
			if(HitLedge)
			{
				SelectedLedge = HitLedge;
				ShowPrompt("Space - Jump to Ledge");
			}else
			{
				SelectedLedge = nullptr;
				HidePrompt("Space - Jump to Ledge");
			}
		}else
		{
			SelectedLedge = nullptr;
			HidePrompt("Space - Jump to Ledge");
		}
	}
	if(bIsClimbing)
	{
		FHitResult LeftOutHit;
		bool bLeftWallHit = GetWorld()->LineTraceSingleByChannel(LeftOutHit, ActorLoc, ActorLoc + GetActorForwardVector() * 50, ECC_WorldStatic, CollisionParams);
		if(bLeftWallHit && LeftOutHit.Actor != SelectedWall)
		{
			AClimbableWall* HitWall = Cast<AClimbableWall>(LeftOutHit.Actor);
			if(!HitWall) return;
			WallTraceInfo = LeftOutHit;
			WallDetected(HitWall);
		}
		FHitResult RightOutHit;
		bool bRightWallHit = GetWorld()->LineTraceSingleByChannel(RightOutHit, ActorLoc, ActorLoc + GetActorForwardVector() * 50, ECC_WorldStatic, CollisionParams);
		if(bRightWallHit && RightOutHit.Actor != SelectedWall)
		{
			AClimbableWall* HitWall = Cast<AClimbableWall>(RightOutHit.Actor);
			if(!HitWall) return;
			WallTraceInfo = RightOutHit;
			WallDetected(HitWall);
		}
	}
	FHitResult WallOutHit;
	if(GetWorld()->LineTraceSingleByChannel(WallOutHit, ActorLoc, ActorLoc + GetActorForwardVector() * 50, ECC_WorldStatic, CollisionParams))
	{
		AClimbableWall* HitWall = Cast<AClimbableWall>(WallOutHit.Actor);
		
		if(HitWall == SelectedWall)
		{
			return;
		}
		if(HitWall)
		{
			WallTraceInfo = WallOutHit;
			WallDetected(HitWall);
			// UE_LOG(LogTemp, Warning, TEXT("Hit"))
			return;
		}
	}
	WallUndetected();
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
	// "TurnRate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	// PlayerInputComponent->BindAxis("TurnRate", this, &AWallClimbJumpCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	// PlayerInputComponent->BindAxis("LookUpRate", this, &AWallClimbJumpCharacter::LookUpAtRate);

	// handle touch devices
	// PlayerInputComponent->BindTouch(IE_Pressed, this, &AWallClimbJumpCharacter::TouchStarted);
	// PlayerInputComponent->BindTouch(IE_Released, this, &AWallClimbJumpCharacter::TouchStopped);

	// VR headset functionality
	// PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AWallClimbJumpCharacter::OnResetVR);
	
	PlayerInputComponent->BindAction("Climb", IE_Pressed, this, &AWallClimbJumpCharacter::WallAttach);
	PlayerInputComponent->BindAction("Grapple", IE_Pressed, this, &AWallClimbJumpCharacter::StartGrapple);
}

void AWallClimbJumpCharacter::LocateTarget()
{
	// float ClosestDistance = 0;
	ALedge* ClosestLedge = nullptr;
	// FVector ClosestLocation;
	for (const auto& Ledge : Ledges)
	{
		// if(GetDistanceTo(Ledge) > 500) continue;
		// if(!Ledge->WasRecentlyRendered(0.1)) continue;
		if(bIsHoldingLedge && TargetLedge == Ledge) continue;
		FVector ClosestPoint;
		const float Distance = Ledge->ActorGetDistanceToCollision(GetActorLocation(), ECC_GameTraceChannel1, ClosestPoint);
		if(Distance <= 0) continue;
		if(!Ledge->IsOnScreen(ClosestPoint)) continue;
		
		if (UKismetMathLibrary::Vector_Distance(ClosestPoint, GetActorLocation()) <= UKismetMathLibrary::Vector_Distance(GrapplePoint, GetActorLocation()) || GrapplePoint == FVector::ZeroVector)
		{
			ClosestLedge = Ledge;
			GrapplePoint = ClosestPoint;
		}
	}
	if(!TargetActor) return;
	if(ClosestLedge)
	{
		TargetLedge = ClosestLedge;
		TargetActor->SetActorLocation(GrapplePoint + FollowCamera->GetForwardVector() * -55);
		TargetActor->SetActorRotation(FRotator(0, UKismetMathLibrary::FindLookAtRotation(GrapplePoint,FollowCamera->GetComponentLocation()).Yaw,0));
		TargetActor->ShowTarget(true);
		// DrawDebugBox(GetWorld(), GrapplePoint, FVector(10,10,10), FColor::Red);
	}
	else
	{
		TargetActor->ShowTarget(false);
	}
}

void AWallClimbJumpCharacter::Detach()
{
	if(AnimController)
	{
		AnimController->bIsClimbing = false;
	}
	GetMesh()->GlobalAnimRateScale = 1.0f;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsClimbing = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	HidePrompt("E - Stop Climbing");
}

void AWallClimbJumpCharacter::StartGrapple()
{
	if(!TargetActor) return;
	if (bIsGrapplePreparing || bIsGrappling) return;
	if (GrapplePoint == FVector::ZeroVector) return;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	CollisionParams.AddIgnoredActor(TargetActor);
	FVector LedgeLocation = TargetLedge->GetActorLocation();
	FHitResult FrontOutHit;
	FVector StartPos = GetActorLocation();
	StartPos.Z = LedgeLocation.Z;
	FVector EndPos = LedgeLocation;
	bool FrontHit = GetWorld()->LineTraceSingleByChannel(FrontOutHit, StartPos, EndPos, ECC_GameTraceChannel1, CollisionParams);
	DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Blue, false, 10, 0, 2);
	if(!FrontHit || !Cast<ALedge>(FrontOutHit.Actor)) return;
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 3, FColor::White,FString("Hit ledge"));
	}
	bIsGrapplePreparing = true;
	WallTraceInfo = FrontOutHit;
	bIsRotating = true;
	GetWorld()->GetTimerManager().SetTimer(GrappleTimerH, this, &AWallClimbJumpCharacter::Grapple, 3.0f);
}

void AWallClimbJumpCharacter::Grapple()
{
	GetWorld()->GetTimerManager().ClearTimer(GrappleTimerH);
	bIsGrappling = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	GrapplePoint.Z -= GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
}

void AWallClimbJumpCharacter::GrappleTravel(float DeltaTime)
{
	const FVector CurrentLocation = GetActorLocation();
	// FRotator TargetDirection = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, GrapplePoint);
	// SetActorLocation(CurrentLocation + (GrapplePoint - CurrentLocation) * 0.1);
	const float Distance = UKismetMathLibrary::Vector_Distance(GrapplePoint, CurrentLocation);
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 3, FColor::White,FString("Distance: ")+FString::SanitizeFloat(Distance));
	}
	
	if(UKismetMathLibrary::Vector_Distance(GrapplePoint, CurrentLocation) > 1)
	{
		SetActorLocation(UKismetMathLibrary::VInterpTo(GetActorLocation(), GrapplePoint, DeltaTime, 1));
	}
	else
	{
		if(AnimController)
		{
			AnimController->bIsHolding = true;
		}
		CurrentLedge = TargetLedge;
		bIsGrappling = false;
		bIsHoldingLedge = true;
		bIsGrapplePreparing = false;
	}
}

void AWallClimbJumpCharacter::WallAttach()
{
	if(bIsClimbing)
	{
		ShowPrompt("E - Climb");
		Detach();
	}
	else if(SelectedWall)
	{
		if(AnimController)
		{
			AnimController->bIsClimbing = true;
		}
		bIsClimbing = true;
		bIsRotating = true;
		GetCharacterMovement()->MaxFlySpeed = 100;
		GetCharacterMovement()->BrakingDecelerationFlying = 80;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately();
		
		ShowPrompt("E - Stop Climbing");
	}
}

void AWallClimbJumpCharacter::WallDetected(AClimbableWall* NewWall)
{
	SelectedWall = NewWall;
	// UE_LOG(LogTemp, Warning, TEXT("Attempting to WallDetected"))
	if(bIsClimbing)
	{
		bIsRotating = true;
		return;
	}
	// UE_LOG(LogTemp, Warning, TEXT("Firing BP event"))
	ShowPrompt("E - Climb");
}

void AWallClimbJumpCharacter::WallUndetected()
{
	if(!SelectedWall)
	{
		return;
	}
	SelectedWall = nullptr;
	if(bIsClimbing)
	{
		Detach();
	}
	else
	{
		HidePrompt("E - Climb");
	}
}

void AWallClimbJumpCharacter::ShowPrompt(FString NewText)
{
	if(!PromptWidget) return;
	if(CurrentPrompt == NewText) return;
	CurrentPrompt = NewText;
	PromptWidget->ShowPrompt(FText::FromString(NewText));
}

void AWallClimbJumpCharacter::HidePrompt(FString NewText)
{
	if(!PromptWidget) return;
	if(CurrentPrompt != NewText) return;
	CurrentPrompt = nullptr;
	PromptWidget->HidePrompt();
}

// void AWallClimbJumpCharacter::OnResetVR()
// {
// 	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
// }
//
// void AWallClimbJumpCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
// {
// 		Jump();
// }
//
// void AWallClimbJumpCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
// {
// 		StopJumping();
// }

// void AWallClimbJumpCharacter::TurnAtRate(float Rate)
// {
// 	// calculate delta for this frame from the rate information
// 	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
// }
//
// void AWallClimbJumpCharacter::LookUpAtRate(float Rate)
// {
// 	// calculate delta for this frame from the rate information
// 	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
// }

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
			// FHitResult reverse;
			// WallTraceInfo.GetReversedHit(reverse);
			// SetActorRotation(WallTraceInfo.Normal.Rotation(), ETeleportType::None);
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
	if(CurrentLedge)
	{
		UE_LOG(LogTemp, Warning, TEXT("current ledge"));
		bIsHoldingLedge = false;
		bIsRotating = false;
		CurrentLedge = nullptr;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		if(!RightLedge && MoveDirection > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("right ledge"));
			if(AnimController)
			{
				AnimController->JumpDirection = EJumpDirection::Right;
				AnimController->IsJumpingOff = true;
			}
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * 970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			// CurrentLedge = nullptr;
			// return;
		}
		else if(!LeftLedge && MoveDirection < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("left ledge"));
			if(AnimController)
			{
				AnimController->JumpDirection = EJumpDirection::Left;
				AnimController->IsJumpingOff = true;
			}
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * -970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			// CurrentLedge = nullptr;
			// return;
		}
		else
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		if(AnimController)
		{
			AnimController->bIsHolding = false;
		}
		HidePrompt("Space - Let Go");
	}
	else if(SelectedLedge)
	{
		if(!TargetActor) return;
		FVector HangLocation = GetMesh()->GetSocketLocation("hand_Socket");
		FHitResult FrontOutHit;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);
		CollisionParams.AddIgnoredActor(TargetActor);
		FVector StartPos = GetActorLocation() + GetActorUpVector() * 140;
		FVector EndPos = StartPos + GetActorForwardVector() * 40;
		bool FrontHit = GetWorld()->LineTraceSingleByChannel(FrontOutHit, StartPos, EndPos, ECC_GameTraceChannel1, CollisionParams);
		DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Blue, false, 10, 0, 2);
		if(!FrontHit || !Cast<ALedge>(FrontOutHit.Actor)) return;
		WallTraceInfo = FrontOutHit;
		HangLocation.Z -= GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		bIsHoldingLedge = true;
		bIsRotating = true;
		CurrentLedge = SelectedLedge;
		if(AnimController)
		{
			AnimController->bIsHolding = true;
			if(GEngine) GEngine->AddOnScreenDebugMessage(-1, 5, FColor::White, FString("Set holding true"));
		}
		SetActorLocation(HangLocation);
		GetCharacterMovement()->MaxFlySpeed = 50;
		GetCharacterMovement()->BrakingDecelerationFlying = 110;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->StopMovementImmediately();
		ShowPrompt("Space - Let Go");
	}
	else if(!bIsClimbing)
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
		if(!TargetActor) return;
		MoveDirection = Value;
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);
		CollisionParams.AddIgnoredActor(TargetActor);
		// CollisionParams.AddIgnoredActor(CurrentLedge);
		FVector rightStartPos = GetActorLocation() + (GetActorRightVector() * 30) + (GetActorUpVector() * 120);
		FVector rightEndPos = rightStartPos + GetActorForwardVector() * 40;
		FVector leftStartPos = GetActorLocation() + (GetActorRightVector() * -30) + (GetActorUpVector() * 120);
		FVector leftEndPos = leftStartPos + GetActorForwardVector() * 40;
		DrawDebugLine(GetWorld(), rightStartPos, rightEndPos, FColor::Blue, false, 0.5, 0, 2);
		DrawDebugLine(GetWorld(), leftStartPos, leftEndPos, FColor::Green, false, 0.5, 0, 2);
		// if(Value == 0)
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("Moving Nowhere"))
		// 	RightLedge = nullptr;
		// 	LeftLedge = nullptr;
		// 	if(animController)
		// 	{
		// 		animController->Direction = 0;
		// 	}
		// }
		if(Value > 0)
		{
			FHitResult rightOutHit;
			bool hitRight = GetWorld()->LineTraceSingleByChannel(rightOutHit, rightStartPos, rightEndPos, ECC_GameTraceChannel1, CollisionParams);
			if (hitRight)
			{
				// UE_LOG(LogTemp, Warning, TEXT("Moving Right"))
				ALedge* HitLedge = Cast<ALedge>(rightOutHit.Actor);
				if(HitLedge)
				{
					RightLedge = HitLedge;
					if(AnimController)
					{
						AnimController->Direction = Value;
						AddMovementInput(GetActorRightVector(), Value, false);
					}
				}
				else
				{
					RightLedge = nullptr;
					if(AnimController)
					{
						AnimController->Direction = 0;
					}
				}
			}
			else
			{
				RightLedge = nullptr;
				if(AnimController)
				{
					AnimController->Direction = 0;
				}
			}
		}
		else if(Value < 0)
		{
			FHitResult leftOutHit;
			bool hitLeft = GetWorld()->LineTraceSingleByChannel(leftOutHit, leftStartPos, leftEndPos, ECC_GameTraceChannel1, CollisionParams);
			if (hitLeft)
			{
				// UE_LOG(LogTemp, Warning, TEXT("Moving Right"))
				ALedge* HitLedge = Cast<ALedge>(leftOutHit.Actor);
				if(HitLedge)
				{
					LeftLedge = HitLedge;
					if(AnimController)
					{
						AnimController->Direction = Value;
						AddMovementInput(GetActorRightVector(), Value, false);
					}
				}
				else
				{
					LeftLedge = nullptr;
					if(AnimController)
					{
						AnimController->Direction = 0;
					}
				}
			}
			else
			{
				LeftLedge = nullptr;
				if(AnimController)
				{
					AnimController->Direction = 0;
				}
			}
		}
		else
		{
			LeftLedge = nullptr;
			RightLedge = nullptr;
			if(AnimController)
			{
				AnimController->Direction = 0;
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
		// SetActorRotation(SelectedWall->GetActorRotation(), ETeleportType::None);
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
