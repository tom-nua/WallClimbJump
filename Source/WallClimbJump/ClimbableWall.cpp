// Fill out your copyright notice in the Description page of Project Settings.


#include "ClimbableWall.h"

#include "WallClimbJumpCharacter.h"
#include "Components/BoxComponent.h"

// Sets default values
AClimbableWall::AClimbableWall()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	interactOverlap = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(interactOverlap);
}

// Called when the game starts or when spawned
void AClimbableWall::BeginPlay()
{
	Super::BeginPlay();
	interactOverlap->OnComponentBeginOverlap.AddDynamic(this, &AClimbableWall::OnComponentBeginOverlap);
	interactOverlap->OnComponentEndOverlap.AddDynamic(this, &AClimbableWall::OnComponentEndOverlap);
}

void AClimbableWall::OnComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	AWallClimbJumpCharacter* character = Cast<AWallClimbJumpCharacter>(OtherActor);
	if (!character)
	{
		UE_LOG(LogTemp, Warning, TEXT("Not character"))
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Firing character ShowPrompt"))
	character->ShowPrompt();

}

void AClimbableWall::OnComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AWallClimbJumpCharacter* character = Cast<AWallClimbJumpCharacter>(OtherActor);
	if (!character)
	{
		return;
	}
	character->HidePrompt();
}

// Called every frame
void AClimbableWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

