// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallClimbJumpCharacter.h"

#include "CharAnimInstance.h"
#include "ClimbableWall.h"
#include "DrawDebugHelpers.h"
#include "HeadMountedDisplayFunctionLibrary.h"
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
	GetCharacterMovement()->MaxFlySpeed = 100;
	GetCharacterMovement()->BrakingDecelerationFlying = 80;

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
	FHitResult outHit;
	FCollisionQueryParams collisionParams;
	collisionParams.AddIgnoredActor(this);
	FVector actorLoc = GetActorLocation();
	DrawDebugLine(GetWorld(), actorLoc, actorLoc + GetActorForwardVector() * 100, FColor::Green, false, 1, 0, 5);
	if(GetWorld()->LineTraceSingleByChannel(outHit, actorLoc, actorLoc + GetActorForwardVector() * 100, ECC_WorldStatic, collisionParams))
	{
		AClimbableWall* HitWall = Cast<AClimbableWall>(outHit.Actor);
		if(HitWall == selectedWall)
		{
			return;
		}
		if(HitWall)
		{
			ShowPrompt(HitWall);
			UE_LOG(LogTemp, Warning, TEXT("Hit"))
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
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

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
		animController->isClimbing = false;
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsClimbing = false;
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
			animController->isClimbing = true;
		}
		bIsClimbing = true;
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
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		if(bIsClimbing)
		{
			SetActorRotation(selectedWall->GetActorRotation(), ETeleportType::None);
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

void AWallClimbJumpCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		if(bIsClimbing)
		{
			SetActorRotation(selectedWall->GetActorRotation(), ETeleportType::None);
			AddMovementInput(GetActorRightVector(), Value, false);
		}
		else
		{
			// get right vector 
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
			// add movement in that direction
			AddMovementInput(Direction, Value);
		}
	}
}
