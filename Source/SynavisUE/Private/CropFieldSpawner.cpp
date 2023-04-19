// Fill out your copyright notice in the Description page of Project Settings.


#include "CropFieldSpawner.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"

// Sets default values
ACropFieldSpawner::ACropFieldSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	CropField = CreateDefaultSubobject<UBoxComponent>(TEXT("Space Bounds"));
}

void ACropFieldSpawner::Spawn(TArray<FVector> Points, TArray<FVector> Normals, TArray<int> Triangles,
	TArray<float> Scalars, float Min, float Max)
{
	UProceduralMeshComponent* mesh = NewObject<UProceduralMeshComponent>(this);

}


// Called when the game starts or when spawned
void ACropFieldSpawner::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACropFieldSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for(USceneComponent* component : SubComponents)
	{
		// retrieve the last time the component was rendered
		

  }
}

