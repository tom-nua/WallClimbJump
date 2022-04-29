// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallClimbJumpCharacter.h"
#include "CharAnimInstance.h"
#include "ClimbableWall.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
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
	HoldOffset = UKismetMathLibrary::MakeRelativeTransform(GetActorTransform(), GetMesh()->GetSocketTransform("hand_Socket")).GetLocation();
	Super::BeginPlay();
	if(PromptWidgetClass)
	{
		UUserWidget* userWidget = CreateWidget(GetWorld()->GetFirstPlayerController(), PromptWidgetClass);
		if(!userWidget) return;
		userWidget->AddToViewport(0);
		PromptWidget = Cast<UUIWidget>(userWidget);		
	}
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance)
	{
		AnimController = Cast<UCharAnimInstance>(AnimInstance);
	}
	if(TargetActorClass)
	{
		TargetActor = Cast<AGrappleTarget>(GetWorld()->SpawnActor(TargetActorClass));
	}
	for(TActorIterator<ALedge> It(GetWorld()); It; ++It)
	{
		ALedge* Ledge = *It;
		Ledges.Add(Ledge);
	}
}

void AWallClimbJumpCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(!TargetActor) return;
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
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	CollisionParams.AddIgnoredActor(TargetActor);
	FVector ActorLoc = GetActorLocation();

	if(bIsRotating && (bIsHoldingLedge && CurrentLedge || bIsClimbing || bIsGrapplePreparing))
	{
		const float ForwardDotProduct = FVector::DotProduct(RotateNormal, GetActorRightVector());
		// UE_LOG(LogTemp, Warning, TEXT("Forward:%f"), ForwardDotProduct);
		if(ForwardDotProduct < 0)
		{
			if(ForwardDotProduct > -0.020000)
			{
				bIsRotating = false;
			}
			else
			{
				FRotator CurrentRot = GetActorRotation();
				SetActorRotation(CurrentRot.Add(0, 1, 0));
			}
		}else if(ForwardDotProduct > 0)
		{
			if(ForwardDotProduct < 0.020000)
			{
				bIsRotating = false;
			}
			else
			{
				FRotator currentRot = GetActorRotation();
				SetActorRotation(currentRot.Add(0, -1, 0));
			}
		}
	}
	if (bIsGrappling)
	{
		TargetActor->ShowTarget(false);
		GrappleTravel(DeltaTime);
		return;
	}
	if(bIsGrapplePreparing)
	{
		TargetActor->ShowTarget(false);
		return;
	}
	LocateTarget();
	if(!bIsHoldingLedge)
	{
		FVector StartPos = ActorLoc + GetActorForwardVector() * 40;
		FVector EndPos = StartPos + GetActorUpVector() * 140;
		// DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Red, false, -1, 0, 3);
		FHitResult LedgeOutHit;
		if(GetWorld()->SweepSingleByChannel(LedgeOutHit, StartPos, EndPos, GetActorRotation().Quaternion(), ECC_GameTraceChannel1, CapsuleCollisionShape, CollisionParams))
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
		DrawDebugCapsule(GetWorld(), StartPos + GetActorUpVector() * 70, 70, 14, GetActorRotation().Quaternion(), FColor::Green, false, -1, 0, 3);
	}
	if(bIsClimbing)
	{
		FHitResult LeftOutHit;
		FVector RayEnd = ActorLoc + GetActorForwardVector() * 50;
		DrawDebugLine(GetWorld(), RayEnd + GetActorRightVector() * -40, RayEnd, FColor::Red, false, -1, 0, 3);
		DrawDebugLine(GetWorld(), RayEnd + GetActorRightVector() * 40, RayEnd, FColor::Red, false, -1, 0, 3);
		bool bLeftWallHit = GetWorld()->LineTraceSingleByChannel(LeftOutHit, RayEnd + GetActorRightVector() * -40, RayEnd, ECC_WorldStatic, CollisionParams);
		if(bLeftWallHit && LeftOutHit.Normal != LeftWallNormal)
		{
			AClimbableWall* HitWall = Cast<AClimbableWall>(LeftOutHit.Actor);
			if(HitWall)
			{
				RotateNormal = LeftOutHit.ImpactNormal;
				bIsRotating = true;
			}
		}
		FHitResult RightOutHit;
		bool bRightWallHit = GetWorld()->LineTraceSingleByChannel(RightOutHit, RayEnd + GetActorRightVector() * 40, RayEnd, ECC_WorldStatic, CollisionParams);
		if(bRightWallHit && RightOutHit.Normal != RightWallNormal)
		{
			AClimbableWall* HitWall = Cast<AClimbableWall>(RightOutHit.Actor);
			if(HitWall)
			{
				RotateNormal = RightOutHit.ImpactNormal;
				bIsRotating = true;
			}
		}
	}
	DrawDebugLine(GetWorld(), ActorLoc, ActorLoc + GetActorForwardVector() * 50, FColor::Green, false, -1, 0, 3);
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
			RotateNormal = WallOutHit.ImpactNormal;
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
	ALedge* ClosestLedge = nullptr;
	GrapplePoint = FVector::ZeroVector;
	if(CurrentLedge)
	{
		DrawDebugSphere(GetWorld(), CurrentLedge->GetActorLocation(), 20, 12, FColor::Blue, false, -1);
	}
	for (const auto& Ledge : Ledges)
	{
		if(bIsHoldingLedge && CurrentLedge == Ledge)
		{
			continue;
		}
		DrawDebugSphere(GetWorld(), GrapplePoint, 20, 12, FColor::Green, false, -1);
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
	if(ClosestLedge)
	{
		TargetLedge = ClosestLedge;
		TargetActor->SetActorLocation(GrapplePoint + FollowCamera->GetForwardVector() * -55);
		TargetActor->SetActorRotation(FRotator(0, UKismetMathLibrary::FindLookAtRotation(GrapplePoint,FollowCamera->GetComponentLocation()).Yaw,0));
		TargetActor->ShowTarget(true);
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
	FHitResult FrontOutHit;
	FVector StartPos = GetActorLocation();
	StartPos.Z = GrapplePoint.Z;
	FVector EndPos = GrapplePoint + UKismetMathLibrary::GetDirectionUnitVector(StartPos, GrapplePoint) * 1;
	bool FrontHit = GetWorld()->LineTraceSingleByChannel(FrontOutHit, StartPos, EndPos, ECC_GameTraceChannel1, CollisionParams);
	DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Blue, false, 10, 0, 2);
	if(!FrontHit || !Cast<ALedge>(FrontOutHit.Actor)) return;
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 3, FColor::White, FString("Hit ledge"));
	}
	bIsGrapplePreparing = true;
	GrappleNormal = FrontOutHit.ImpactNormal;
	RotateNormal = GrappleNormal;
	// RotateNormal.X += 90;
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 3, FColor::White, RotateNormal.ToCompactString());
	}
	bIsRotating = true;
	if(AnimController)
	{
		AnimController->bIsGrappling = true;
	}
	GetWorld()->GetTimerManager().SetTimer(GrappleTimerH, this, &AWallClimbJumpCharacter::Grapple, 3.0f);
}

void AWallClimbJumpCharacter::Grapple()
{
	GetWorld()->GetTimerManager().ClearTimer(GrappleTimerH);
	bIsGrappling = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	GrapplePoint.Z += HoldOffset.Z;
}

void AWallClimbJumpCharacter::GrappleTravel(const float DeltaTime)
{
	RotateNormal = GrappleNormal;
	if(GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 3, FColor::White, RotateNormal.ToCompactString());
	}
	bIsRotating = true;
	const FVector NewLocation = UKismetMathLibrary::VInterpTo(GetActorLocation(), GrapplePoint, DeltaTime, 10);
	if(NewLocation != GrapplePoint)
	{
		SetActorLocation(NewLocation);
	}
	else
	{
		if(AnimController)
		{
			AnimController->bIsGrappling = false;
			AnimController->bIsHolding = true;
		}
		CurrentLedge = TargetLedge;
		bIsGrappling = false;
		bIsHoldingLedge = true;
		bIsGrapplePreparing = false;
		GrabLedge(NewLocation);
	}
}

void AWallClimbJumpCharacter::GrabLedge(const FVector HangLocation)
{
	bIsHoldingLedge = true;
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
	if(bIsClimbing)
	{
		bIsRotating = true;
		return;
	}
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

void AWallClimbJumpCharacter::MoveForward(const float Value)
{
	if (Controller && Value != 0.0f)
	{
		if(bIsHoldingLedge || bIsGrapplePreparing || bIsGrappling) return;
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		if(bIsClimbing)
		{
			GetMesh()->GlobalAnimRateScale = 1.0f;
			AddMovementInput(GetActorUpVector(), Value, false);
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
				AnimController->bIsJumpingOff = true;
			}
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * 970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		else if(!LeftLedge && MoveDirection < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("left ledge"));
			if(AnimController)
			{
				AnimController->JumpDirection = EJumpDirection::Left;
				AnimController->bIsJumpingOff = true;
			}
			GetCharacterMovement()->AddImpulse(GetActorRightVector() * -970, true);
			GetCharacterMovement()->SetMovementMode(MOVE_Falling);
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
		FVector EndPos = StartPos + GetActorForwardVector() * 60;
		bool FrontHit = GetWorld()->LineTraceSingleByChannel(FrontOutHit, StartPos, EndPos, ECC_GameTraceChannel1, CollisionParams);
		DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Blue, false, 10, 0, 2);
		if(!FrontHit || !Cast<ALedge>(FrontOutHit.Actor)) return;
		RotateNormal = FrontOutHit.ImpactNormal;
		HangLocation.Z -= GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		bIsRotating = true;
		CurrentLedge = SelectedLedge;
		GrabLedge(HangLocation);
	}
	else if(!bIsClimbing && !bIsGrappling && !bIsGrapplePreparing)
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
		FVector RightStartPos;
		RightStartPos = GetActorLocation() + (GetActorRightVector() * 30) + (GetActorUpVector() * 120);
		FVector RightEndPos = RightStartPos + GetActorForwardVector() * 40;
		FVector LeftStartPos = GetActorLocation() + (GetActorRightVector() * -30) + (GetActorUpVector() * 120);
		FVector LeftEndPos = LeftStartPos + GetActorForwardVector() * 40;
		DrawDebugLine(GetWorld(), RightStartPos, RightEndPos, FColor::Blue, false, 0.5, 0, 2);
		DrawDebugLine(GetWorld(), LeftStartPos, LeftEndPos, FColor::Green, false, 0.5, 0, 2);
		if(Value > 0)
		{
			FHitResult RightOutHit;
			bool bHitRight;
			bHitRight = GetWorld()->LineTraceSingleByChannel(RightOutHit, RightStartPos, RightEndPos, ECC_GameTraceChannel1,
			                                                 CollisionParams);
			if (bHitRight)
			{
				ALedge* HitLedge = Cast<ALedge>(RightOutHit.Actor);
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
			FHitResult LeftOutHit;
			bool bHitLeft;
			bHitLeft = GetWorld()->LineTraceSingleByChannel(LeftOutHit, LeftStartPos, LeftEndPos, ECC_GameTraceChannel1,
			                                                CollisionParams);
			if (bHitLeft)
			{
				ALedge* HitLedge = Cast<ALedge>(LeftOutHit.Actor);
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
	if(bIsGrappling || bIsGrapplePreparing) return;
	if(bIsClimbing)
	{
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
