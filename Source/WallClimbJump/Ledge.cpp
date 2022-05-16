// Fill out your copyright notice in the Description page of Project Settings.


#include "Ledge.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ALedge::ALedge()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

bool ALedge::IsOnScreen(FVector PointLocation)
{
	FVector2D ScreenLocation;
	UGameplayStatics::ProjectWorldToScreen(GetWorld()->GetFirstPlayerController(), PointLocation, ScreenLocation, false);
	const FVector2D ScreenSize = UWidgetLayoutLibrary::GetViewportSize(this);
	if(ScreenLocation.X < 0 || ScreenLocation.Y < 0) return false;
	if(ScreenLocation.X > ScreenSize.X || ScreenLocation.Y > ScreenSize.Y) return false;
	
	return true;
}

// Called every frame
void ALedge::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

