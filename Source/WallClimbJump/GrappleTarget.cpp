// Fill out your copyright notice in the Description page of Project Settings.


#include "GrappleTarget.h"

#include "Components/WidgetComponent.h"

// Sets default values
AGrappleTarget::AGrappleTarget()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AGrappleTarget::BeginPlay()
{
	Super::BeginPlay();
	WidgetComponent = FindComponentByClass<UWidgetComponent>();
}

// Called every frame
void AGrappleTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGrappleTarget::ShowTarget(bool Show)
{
	if(!WidgetComponent) return;
	WidgetComponent->SetVisibleFlag(Show);
}

