// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CropFieldSpawner.generated.h"


class UProceduralMeshComponent;
UCLASS()
class SYNAVISUE_API ACropFieldSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACropFieldSpawner();

	UFUNCTION(BlueprintCallable, Category = "Field")
		void Spawn(TArray<FVector> Points, TArray<FVector> Normals, TArray<int> Triangles, TArray<float> Scalars, float Min, float Max);

	UPROPERTY()
  class UPrimitiveComponent* CropField;

	TArray<USceneComponent*> SubComponents;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
