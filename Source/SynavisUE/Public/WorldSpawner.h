// Copyright Dirk Norbert Helmrich, 2023

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldSpawner.generated.h"


USTRUCT(BlueprintType)
struct FObjectSpawnInstance
{
	GENERATED_BODY()
  FString Name, ClassName;
  FVector Location;
  UObject* Object;
  UClass* Class;
  int Identity;
};

class UProceduralMeshComponent;
class UDynamicMaterialInstance;
struct FStreamableHandle;

UCLASS()
class SYNAVISUE_API AWorldSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWorldSpawner();

	UFUNCTION(BlueprintCallable, Category = "Field", meta = (AutoCreateRefTerm = "Tangents, TexCoords"))
		void SpawnProcMesh(TArray<FVector> Points, TArray<FVector> Normals, TArray<int> Triangles, TArray<float> Scalars, float Min, float Max,
		           TArray<FVector2D> TexCoords, TArray<FProcMeshTangent> Tangents);

	UPROPERTY()
  class UPrimitiveComponent* CropField;

  UPROPERTY(BlueprintReadOnly, Category = "Management")
  TArray<FObjectSpawnInstance> SpawnedObjects;

  UPROPERTY(BlueprintReadOnly, Category = "Management")
  bool AllowDefaultParameters = false;

	UFUNCTION(BlueprintPure, Category = "Coupling")
	TArray<FString> GetNamesOfSpawnableTypes();
	

	UFUNCTION()
	void ReceiveStreamingCommunicatorRef();

	TArray<USceneComponent*> SubComponents;

	// a function that returns a StaticClass from a name
	UClass* GetClassFromName(FString ClassName);

	UTexture2D* CreateTexture2DFromData(TArray<uint8> Data, int Width, int Height);
	UDynamicMaterialInstance* GenerateInstanceFromName(FString InstanceName);

	// This function spawns a pre-registered object from a JSON description
	// The description must contain a "ClassName" field that contains the name of the class to spawn
	// The description must adhere with the internal spawn parameter map
	// @return The name of the spawned object, this should also just appear in the scene.
	FString SpawnObject(const FJsonObject& Description);

	//UPROPERTY(EditAnywhere, Category = "Field")
  TMap<FString, UDynamicMaterialInstance*> MaterialInstances;

	void ReceiveStreamingCommunicatorRef(ASynavisDrone* inDroneRef);

	const FJsonObject* GetAssetCacheTemp() const {return AssetCache.Get();}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

  ASynavisDrone* DroneRef;
	void MessageToClient(FString Message);

	TMap<FName, FTopLevelAssetPath> MeshAssetCache;
	TMap<FName, FTopLevelAssetPath> MaterialAssetPathCache;

	TSharedPtr<FJsonObject> AssetCache;

	TArray<TSharedPtr<FStreamableHandle>> StreamableHandles;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
