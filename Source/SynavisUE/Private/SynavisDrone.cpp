// Copyright Dirk Norbert Helmrich, 2023

#include "SynavisDrone.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Engine/Scene.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Components/BoxComponent.h"
#include "DSP/PassiveFilter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/UserWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Async/Async.h"
#include "InstancedFoliageActor.h"
#include "Landscape.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Engine/LevelStreaming.h"
#include "Serialization/JsonReader.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "WorldSpawner.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

THIRD_PARTY_INCLUDES_START
#include <limits>
THIRD_PARTY_INCLUDES_END

inline float GetColor(const FColor& c, uint32 i)
{
  switch (i)
  {
  default:
  case 0:
    return c.R;
  case 1:
    return c.G;
  case 2:
    return c.B;
  case 3:
    return c.A;
  }
}

void ASynavisDrone::ParseInput(FString Descriptor)
{
  // reinterpret the message as ASCII
  const auto* Data = reinterpret_cast<const char*>(*Descriptor);
  // parse into FString
  FString Message(UTF8_TO_TCHAR(Data));
  // remove line breaks
  Message.ReplaceInline(TEXT("\r"), TEXT(""));
  Message.ReplaceInline(TEXT("\n"), TEXT(""));

  UE_LOG(LogTemp, Warning, TEXT("M: %s"), *Message);
  if (Descriptor[0] == '{' && Descriptor[Descriptor.Len() - 1] == '}')
  {
    TSharedPtr<FJsonObject> Jason = MakeShareable(new FJsonObject());
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Descriptor);
    FJsonSerializer::Deserialize(Reader, Jason);

    UE_LOG(LogTemp, Warning, TEXT("Received %s"), *Descriptor);
    // check if the message is a geometry message
    if (Jason->HasField("type"))
    {
      auto type = Jason->GetStringField("type");
      if (type == "geometry")
      {
        Points.Empty();
        Normals.Empty();
        Triangles.Empty();
        UVs.Empty();
        Scalars.Empty();
        Tangents.Empty();
        // this is the partitioned transmission of the geometry
        // We will receive the buffers in chunks and with individual size warnings
        // Here we prompt the World Spawner to create a new geometry container
        WorldSpawner->SpawnObject(Jason);
      }
      else if (type == "directbase64")
      {
        FBase64 Base64;
        // this is the direct transmission of the geometry
        // this means that the properties contain the buffers
        Points.Empty();
        Normals.Empty();
        Triangles.Empty();
        FString id;
        // check which geometry this message is for
        if (Jason->HasField("id"))
        {
          id = Jason->GetStringField("id");
        }
        // fetch the geometry from the world
        if (!WorldSpawner)
        {
          SendError("No WorldSpawner found");
          UE_LOG(LogTemp, Error, TEXT("No WorldSpawner found"));
        }
        // pre-allocate the data destination
        TArray<uint8> Dest;
        // determine the maximum size of the data
        uint64_t MaxSize = 0;
        for (auto Field : Jason->Values)
        {
          if (Field.Key == "type")
            continue;
          auto& Value = Field.Value;
          if (Value->Type == EJson::String)
          {
            auto Source = Value->AsString();
            if (Base64.GetDecodedDataSize(Source) > MaxSize)
              MaxSize = Source.Len();
          }
        }
        // allocate the destination buffer
        Dest.SetNumUninitialized(MaxSize);

        // get the json property for the points
        auto points = Jason->GetStringField("points");
        // decode the base64 string
        Base64.Decode(points, Dest);
        // copy the data into the points array
        Points.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
        FMemory::Memcpy(Points.GetData(), Dest.GetData(), Dest.Num());
        auto normals = Jason->GetStringField("normals");
        Dest.Reset();
        Base64.Decode(normals, Dest);
        Normals.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
        FMemory::Memcpy(Normals.GetData(), Dest.GetData(), Dest.Num());
        auto triangles = Jason->GetStringField("triangles");
        Dest.Reset();
        Base64.Decode(triangles, Dest);
        Triangles.SetNumUninitialized(Dest.Num() / sizeof(int), true);
        FMemory::Memcpy(Triangles.GetData(), Dest.GetData(), Dest.Num());
        UVs.Reset();
        Dest.Reset();
        auto uvs = Jason->GetStringField("texcoords");
        Base64.Decode(uvs, Dest);
        UVs.SetNumUninitialized(Dest.Num() / sizeof(FVector2D), true);
        FMemory::Memcpy(UVs.GetData(), Dest.GetData(), Dest.Num());
        // see if there are scalars
        auto scalars = Jason->GetStringField("scalars");
        if (scalars.Len() > 0)
        {
          Dest.Reset();
          Base64.Decode(scalars, Dest);
          Scalars.SetNumUninitialized(Dest.Num() / sizeof(float), true);
          // for range calculation, we must move through the data manually
          auto ScalarData = reinterpret_cast<float*>(Dest.GetData());
          auto Min = std::numeric_limits<float>::max();
          auto Max = std::numeric_limits<float>::min();
          for (size_t i = 0; i < Dest.Num() / sizeof(float); i++)
          {
            auto Value = ScalarData[i];
            if (Value < Min)
              Min = Value;
            if (Value > Max)
              Max = Value;
            Scalars[i] = Value;
          }
        }
        // see if there are tangents
        auto tangents = Jason->GetStringField("tangents");
        if (tangents.Len() > 0)
        {
          Dest.Reset();
          Base64.Decode(tangents, Dest);
          Tangents.SetNumUninitialized(Dest.Num() / sizeof(FProcMeshTangent), true);
          FMemory::Memcpy(Tangents.GetData(), Dest.GetData(), Dest.Num());
        }
        else
        {
          // calculate tangents
          Tangents.SetNumUninitialized(Points.Num(), true);
          for (int p = 0; p < Points.Num(); ++p)
          {
            FVector TangentX = FVector::CrossProduct(Normals[p], FVector(0, 0, 1));
            TangentX.Normalize();
            Tangents[p] = FProcMeshTangent(TangentX, false);
          }
        }
        WorldSpawner->SpawnProcMesh(Points, Normals, Triangles, Scalars, 0.0, 1.0, UVs, Tangents);
      }
      else if (type == "parameter")
      {
        auto* Target = this->GetObjectFromJSON(Jason.Get());
        ApplyJSONToObject(Target, Jason.Get());
      }
      else if (type == "query")
      {
        if (!Jason->HasField("object"))
        {
          if (Jason->HasField("spawn"))
          {
            FString spawn = Jason->GetStringField("spawn");
            if (WorldSpawner)
            {
              auto cache = WorldSpawner->GetAssetCacheTemp();

              if (spawn == "any")
              {
                // return names of all available assets
                FString message = "{\"type\":\"query\",\"name\":\"spawn\",\"data\":[";
                TArray<FString> Names = WorldSpawner->GetNamesOfSpawnableTypes();
                for (int i = 0; i < Names.Num(); ++i)
                {
                  message += FString::Printf(TEXT("\"%s\""), (*Names[i]));
                  if (i < Names.Num() - 1)
                    message += TEXT(",");
                }
                message += "]}";
                this->SendResponse(message);
              }
              else
              {
                // we are still in query mode, so this must mean that spawn parameters should be listed
                if (cache->HasField(spawn))
                {
                  auto asset_json = cache->GetObjectField(spawn);
                  // the asset json already contains all info
                  // serialize
                  FString message;
                  TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<TCHAR>::Create(&message);
                  FJsonSerializer::Serialize(asset_json.ToSharedRef(), Writer);
                  message = FString::Printf(TEXT("{\"type\":\"query\",\"name\":\"spawn\",\"data\":%s}"), *message);
                  this->SendResponse(message);
                }
              }
            }
          }
          else
          {
            // respond with names of all actors
            FString message = "{\"type\":\"query\",\"name\":\"all\",\"data\":[";
            TArray<AActor*> Actors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Actors);
            for (auto i = 0; i < Actors.Num(); ++i)
            {
              message += FString::Printf(TEXT("\"%s\""), *Actors[i]->GetName());
              if (i < Actors.Num() - 1)
                message += TEXT(",");
            }
            message += "]}";
            this->SendResponse(message);
          }
        }
        else
        {
          auto* Target = this->GetObjectFromJSON(Jason.Get());
          if (Target != nullptr)
          {
            FString Name = Target->GetName();
            FString JsonData = ListObjectPropertiesAsJSON(Target);
            FString message = FString::Printf(TEXT("{\"type\":\"query\",\"name\":%s,\"data\":%s}"), *Name, *JsonData);
            this->SendResponse(message);
          }
        }
      }
      else if (type == "track")
      {
        // this is a request to track a property
        // we need values "object" and "property"
        if (!Jason->HasField("object") || !Jason->HasField("property"))
        {
          SendError("track request needs object and property fields");
          UE_LOG(LogTemp, Error, TEXT("track request needs object and property fields"))
        }
        else
        {
          FString ObjectName = Jason->GetStringField("object");
          FString PropertyName = Jason->GetStringField("property");
          auto Object = this->GetObjectFromJSON(Jason.Get());
          if (!Object)
          {
            SendError("track request object not found");
            return;
          }

          // check if we are already tracking this property
          if (this->TransmissionTargets.ContainsByPredicate([Object, PropertyName](const FTransmissionTarget& Target)
            {
              return Target.Object == Object && Target.Property->GetName() == PropertyName;
            }))
          {
            SendError("track request already tracking this property");
            return;
          }

            // check if the property is one of the shortcut properties
            if (PropertyName == "Position" || PropertyName == "Rotation" || PropertyName == "Scale" || PropertyName == "Transform")
            {
              // there is no property to track, but we need to add a transmission target
              TransmissionTargets.Add({ Object, nullptr, EDataTypeIndicator::Transform, FString::Printf(TEXT("%s.%s"), *ObjectName, *PropertyName) });
            }
            else
            {

              auto Property = Object->GetClass()->FindPropertyByName(*PropertyName);

              if (!Property)
              {
                SendError("track request Property not found");
                return;
              }

              this->TransmissionTargets.Add({ Object, Property, this->FindType(Property),
                FString::Printf(TEXT("%s.%s"),*ObjectName,*PropertyName) });
            }
        }
      }
      else if (type == "untrack")
      {
        // this is a request to untrack a property
        // we need values "object" and "property"
        if (!Jason->HasField("object") || !Jason->HasField("property"))
        {
          SendError("untrack request needs object and property fields");
          UE_LOG(LogTemp, Error, TEXT("untrack request needs object and property fields"))
        }
        else
        {
          FString ObjectName = Jason->GetStringField("object");
          FString PropertyName = Jason->GetStringField("property");
          auto Object = this->GetObjectFromJSON(Jason.Get());
          if (!Object)
          {
            SendError("untrack request object not found");
            return;
          }
          auto Property = Object->GetClass()->FindPropertyByName(*PropertyName);
          if (!Property)
          {
            SendError("untrack request Property not found");
            return;
          }
          for (int i = 0; i < this->TransmissionTargets.Num(); ++i)
          {
            if (this->TransmissionTargets[i].Object == Object && this->TransmissionTargets[i].Property == Property)
            {
              this->TransmissionTargets.RemoveAt(i);
              break;
            }
          }
        }
      }
      else if (type == "command")
      {
        // received a command
        FString Name = Jason->GetStringField("name");
        if (Name == "reset")
        {
          // reset the geometry
          Points.Empty();
          Normals.Empty();
          Triangles.Empty();
          UVs.Empty();
        }
        else if (Name == "frametime")
        {
          float frametime = GetWorld()->GetDeltaSeconds();
          FString message = FString::Printf(TEXT("{\"type\":\"frametime\",\"value\":%f}"), frametime);
        }
      }
      else if (type == "info")
      {
        if (Jason->HasField("frametime"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"frametime\":%f}"), GetWorld()->GetDeltaSeconds());
          SendResponse(Response);
        }
        else if (Jason->HasField("memory"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"memory\":%d}"), FPlatformMemory::GetStats().TotalPhysical);
          SendResponse(Response);
        }
        else if (Jason->HasField("fps"))
        {
          const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"fps\":%d}"), static_cast<uint32_t>(FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64())));
          SendResponse(Response);
        }
        else if (Jason->HasField("object"))
        {
          FString RequestedObjectName = Jason->GetStringField("object");
          TArray<AActor*> FoundActors;
        }
      }
      else if (type == "console")
      {
        if (Jason->HasField("command"))
        {
          FString Command = Jason->GetStringField("command");
          UE_LOG(LogTemp, Warning, TEXT("Console command %s"), *Command);
          auto* Controller = GetWorld()->GetFirstPlayerController();
          if (Controller)
          {
            Controller->ConsoleCommand(Command);
          }
        }
      }
      else if (type == "settings")
      {
        // check for settings subobject and put it into member
        if (Jason->HasField("settings"))
        {
          auto Settings = Jason->GetObjectField("settings");
          LoadFromJSON(Message);
        }
      }
      else if (type == "spawn")
      {
        if (this->WorldSpawner)
        {
          auto name = this->WorldSpawner->SpawnObject(Jason);
          SendResponse(FString::Printf(TEXT("{\"type\":\"spawn\",\"name\":\"%s\"}"), *name));
        }
        else
        {
          UE_LOG(LogTemp, Warning, TEXT("No world spawner available"));
          SendError("No world spawner available");
        }
      }
      else if (type == "buffer")
      {
        FString name;
        if (Jason->HasField("start") && Jason->HasField("size"))
        {
          name = Jason->GetStringField("start");
          auto size = Jason->GetIntegerField("size");
          ReceptionName = name;
          if (ReceptionName == "points")
          {
            Points.SetNum(size / sizeof(FVector));
            ReceptionBuffer = reinterpret_cast<uint8*>(Points.GetData());
          }
          else if (ReceptionName == "normals")
          {
            Points.SetNum(size / sizeof(FVector));
            ReceptionBuffer = reinterpret_cast<uint8*>(Normals.GetData());
          }
          else if (ReceptionName == "triangles")
          {
            ReceptionBuffer = reinterpret_cast<uint8*>(Triangles.GetData());
          }
          else if (ReceptionName == "uvs")
          {
            ReceptionBuffer = reinterpret_cast<uint8*>(UVs.GetData());
          }
          else
          {
            UE_LOG(LogTemp, Warning, TEXT("Unknown buffer name %s"), *ReceptionName);
            SendError("Unknown buffer name");
            return;
          }
          SendResponse(FString::Printf(TEXT("{\"type\":\"buffer\",\"name\":\"%s\", \"state\":\"start\"}"), *name));
        }
        else if (Jason->HasField("stop"))
        {
          name = Jason->GetStringField("stop");
          SendResponse(FString::Printf(TEXT("{\"type\":\"buffer\",\"name\":\"%s\", \"state\":\"stop\"}"), *name));
        }
        else
        {
          SendError("buffer request needs start or stop field");
          return;
        }
      }
    }
    else
    {
      UE_LOG(LogTemp, Warning, TEXT("No type field in JSON"));
      SendError("No type field in JSON");
    }
  }
  else
  {
    if (ReceptionName.IsEmpty())
    {
      UE_LOG(LogTemp, Warning, TEXT("Received data is not JSON and we are not waiting for data."));
      SendError("Received data is not JSON and we are not waiting for data.");
    }
    else
    {
      UE_LOG(LogTemp, Warning, TEXT("Received data is not JSON but we are waiting for data."));
      const uint8* data = reinterpret_cast<const uint8*>(*Descriptor);
      auto size = Descriptor.Len() * sizeof(wchar_t);
      FMemory::Memcpy(ReceptionBuffer, data, size);
      ReceptionBuffer += size;
      SendResponse(FString::Printf(TEXT("{\"type\":\"buffer\",\"name\":\"%s\", \"state\":\"transit\"}"), *ReceptionName));
    }
  }
}

void ASynavisDrone::SendResponse(FString Descriptor)
{
  FString Response(reinterpret_cast<TCHAR*>(TCHAR_TO_UTF8(*Descriptor)));
  UE_LOG(LogTemp, Warning, TEXT("Sending response: %s"), *Descriptor);
  OnPixelStreamingResponse.Broadcast(Response);
}

void ASynavisDrone::SendError(FString Message)
{
  FString Response = FString::Printf(TEXT("{\"type\":\"error\",\"message\":%s}"), *Message);
  SendResponse(Response);
}

void ASynavisDrone::ResetSynavisState()
{
  TransmissionTargets.Empty();
}


// Sets default values
ASynavisDrone::ASynavisDrone()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  CoordinateSource = CreateDefaultSubobject<USceneComponent>(TEXT("Root Component"));
  RootComponent = CoordinateSource;
  InfoCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Information Camera"));
  InfoCam->SetupAttachment(RootComponent);
  Flyspace = CreateDefaultSubobject<UBoxComponent>(TEXT("Fly Space"));
  Flyspace->SetBoxExtent({ 100.f,100.f,100.f });

  //Flyspace->SetupAttachment(RootComponent);

  //static ConstructorHelpers::FObjectFinder<UMaterial> Filter(TEXT("Material'/SynavisUE/SegmentationMaterial'"));
  static ConstructorHelpers::FObjectFinder<UMaterial> Filter(TEXT("Material'/SynavisUE/SteeringMaterial.SteeringMaterial'"));
  if (Filter.Succeeded())
  {
    PostProcessMat = Filter.Object;

  }

  static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> InfoTarget(TEXT("TextureRenderTarget2D'/SynavisUE/SceneTarget.SceneTarget'"));
  if (InfoTarget.Succeeded())
  {
    InfoCamTarget = InfoTarget.Object;
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("Could not load one of the textures."));
  }

  static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> SceneTarget(TEXT("TextureRenderTarget2D'/SynavisUE/InfoTarget.InfoTarget'"));
  if (SceneTarget.Succeeded())
  {
    SceneCamTarget = SceneTarget.Object;
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("Could not load one of the textures."));
  }

  SceneCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Rendering Camera"));
  SceneCam->SetupAttachment(RootComponent);
  InfoCam->SetRelativeLocation({ 0,0,0 });
  SceneCam->SetRelativeLocation({ 0, 0, 0 });

  ParamsObject.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
  ParamsObject.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
  ParamsTrace.AddIgnoredActor(this);


  LoadConfig();
  this->SetActorTickEnabled(false);
}

void ASynavisDrone::LoadFromJSON(FString JasonString)
{
  if (JasonString == "")
  {
    FString LevelName = GetLevel()->GetOuter()->GetName();
    FString File = FPaths::GeneratedConfigDir() + UGameplayStatics::GetPlatformName() + TEXT("/") + LevelName + TEXT(".json");
    UE_LOG(LogTemp, Warning, TEXT("Testing for file: %s"), *File);
    if (!FFileHelper::LoadFileToString(JasonString, *File))
    {
      FFileHelper::SaveStringToFile(TEXT("{}"), *File);
      return;
    }
  }
  TSharedPtr<FJsonObject> Jason = MakeShareable(new FJsonObject());
  TSharedRef<TJsonReader<TCHAR>> Reader = FJsonStringReader::Create(JasonString);
  FJsonSerializer::Deserialize(Reader, Jason);
  for (auto Key : Jason->Values)
  {
    if (Key.Key.StartsWith(TEXT("type")))
    {
      // skip network type
      continue;
    }
    auto* prop = GetClass()->FindPropertyByName(FName(*(Key.Key.RightChop(1))));
    if (Key.Key.StartsWith(TEXT("f")))
    {
      FNumericProperty* fprop = CastField<FNumericProperty>(prop);
      if (fprop)
      {
        fprop->SetFloatingPointPropertyValue(fprop->ContainerPtrToValuePtr<void>(this), Key.Value->AsNumber());
      }
    }
    else if (Key.Key.StartsWith(TEXT("i")))
    {
      FNumericProperty* iprop = CastField<FNumericProperty>(prop);
      if (iprop)
      {
        iprop->SetIntPropertyValue(iprop->ContainerPtrToValuePtr<void>(this), (int64)Key.Value->AsNumber());
      }
    }
    else if (Key.Key.StartsWith(TEXT("v")))
    {
      if (!LockNavigation && Key.Key.Contains(TEXT("Position")))
      {
        FVector loc(Key.Value->AsObject()->GetNumberField(TEXT("x")),
          Key.Value->AsObject()->GetNumberField(TEXT("y")),
          Key.Value->AsObject()->GetNumberField(TEXT("z")));
        Flyspace->SetWorldLocation(loc);
      }
      else if (!LockNavigation && Key.Key.Contains(TEXT("Orientation")))
      {
        FRotator rot(Key.Value->AsObject()->GetNumberField(TEXT("p")),
          Key.Value->AsObject()->GetNumberField(TEXT("y")),
          Key.Value->AsObject()->GetNumberField(TEXT("r")));
        InfoCam->SetRelativeRotation(rot);
        SceneCam->SetRelativeRotation(rot);
      }
      else
      {
        FStructProperty* vprop = CastField<FStructProperty>(prop);
        if (vprop)
        {
          TSharedPtr<FJsonObject> Values = Key.Value->AsObject();
          FVector* Output = vprop->ContainerPtrToValuePtr<FVector>(this);
          Output->X = Values->GetNumberField(TEXT("x"));
          Output->Y = Values->GetNumberField(TEXT("y"));
          Output->Z = Values->GetNumberField(TEXT("z"));
        }
      }
    }
    else if (Key.Key.StartsWith(TEXT("b")))
    {
      FBoolProperty* bprop = CastField<FBoolProperty>(prop);
      if (bprop)
      {
        bprop->SetPropertyValue(bprop->ContainerPtrToValuePtr<void>(this), Key.Value->AsBool());
      }
    }
  }
}

FTransform ASynavisDrone::FindGoodTransformBelowDrone()
{
  FTransform Output;
  FHitResult Hit;
  FVector Start = GetActorLocation();
  FVector End = Start - FVector(0, 0, 1000);
  if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECollisionChannel::ECC_WorldStatic, ParamsTrace))
  {
    Output.SetLocation(Hit.ImpactPoint);
  }
  else
  {
    Output = GetActorTransform();
  }
  return Output;
}

FString ASynavisDrone::ListObjectPropertiesAsJSON(UObject* Object)
{
  const UClass* Class = Object->GetClass();
  FString OutputString = TEXT("{");
  for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
  {
    FProperty* Property = *It;
    auto name = Property->GetName();
    auto type = Property->GetCPPType();
    OutputString += TEXT(" \"") + name + TEXT("\": \"") + type + TEXT("\"");
    // check if we are at the end of the list
    if (It->Next)
    {
      OutputString += TEXT(",");
    }
  }
  return OutputString + TEXT(" }");
}

void ASynavisDrone::ApplyJSONToObject(UObject* Object, FJsonObject* JSON)
{
  // received a parameter update
  FString Name = JSON->GetStringField("name");

  auto* Property = GetClass()->FindPropertyByName(*Name);
  if (Property)
  {
    if (Property->IsA(FIntProperty::StaticClass()))
    {
      auto* IntProperty = CastField<FIntProperty>(Property);
      IntProperty->SetPropertyValue_InContainer(Object, JSON->GetIntegerField("value"));
    }
    else if (Property->IsA(FFloatProperty::StaticClass()))
    {
      auto* FloatProperty = CastField<FFloatProperty>(Property);
      FloatProperty->SetPropertyValue_InContainer(Object, JSON->GetNumberField("value"));
    }
    else if (Property->IsA(FBoolProperty::StaticClass()))
    {
      auto* BoolProperty = CastField<FBoolProperty>(Property);
      BoolProperty->SetPropertyValue_InContainer(Object, JSON->GetBoolField("value"));
    }
    else if (Property->IsA(FStrProperty::StaticClass()))
    {
      auto* StringProperty = CastField<FStrProperty>(Property);
      StringProperty->SetPropertyValue_InContainer(Object, JSON->GetStringField("value"));
    }
    // check if property is a vector
    else if (Property->IsA(FStructProperty::StaticClass()))
    {
      auto* StructProperty = CastField<FStructProperty>(Property);
      // check if the struct is a vector via the JSON
      if (JSON->HasField("x") && JSON->HasField("y") && JSON->HasField("z"))
      {
        auto* VectorValue = StructProperty->ContainerPtrToValuePtr<FVector>(Object);
        if (VectorValue)
        {
          VectorValue->X = JSON->GetNumberField("x");
          VectorValue->Y = JSON->GetNumberField("y");
          VectorValue->Z = JSON->GetNumberField("z");
        }
      }
    }
  }
}

UObject* ASynavisDrone::GetObjectFromJSON(FJsonObject* JSON)
{
  FString Name = JSON->GetStringField("object");
  TArray<AActor*> FoundActors;
  UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
  // iterate over all actors
  for (auto* Actor : FoundActors)
  {
    // check if the actor has the same name as the JSON object
    if (Actor->GetName() == Name)
    {
      return Actor;
    }
  }
  // if no actor was found, check if the object is a component
  if (Name.Contains(TEXT(".")))
  {
    FString ActorName = Name.Left(Name.Find(TEXT(".")));
    FString ComponentName = Name.Right(Name.Len() - Name.Find(TEXT(".")) - 1);
    // iterate over all actors
    for (auto* Actor : FoundActors)
    {
      // check if the actor has the same name as the JSON object
      if (Actor->GetName() == ActorName)
      {
        // iterate over all components
        for (auto* Component : Actor->GetComponents())
        {
          // check if the component has the same name as the JSON object
          if (Component->GetName() == ComponentName)
          {
            return Component;
          }
        }
      }
    }
  }
  return nullptr;
}

const bool ASynavisDrone::IsInEditor() const
{
#ifdef WITH_EDITOR
  return true;
#else
  return false;
#endif
}

// Called when the game starts or when spawned
void ASynavisDrone::BeginPlay()
{
  this->SetActorTickEnabled(false);
  Super::BeginPlay();

  InfoCam->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
  SceneCam->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
  SpaceOrigin = Flyspace->GetComponentLocation();
  SpaceExtend = Flyspace->GetScaledBoxExtent();

  Flyspace->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  LoadFromJSON();
  auto* world = GetWorld();
  CallibratedPostprocess = UMaterialInstanceDynamic::Create(PostProcessMat, this);
  InfoCam->AddOrUpdateBlendable(CallibratedPostprocess, 1.f);
  CallibratedPostprocess->SetScalarParameterValue(TEXT("DistanceScale"), DistanceScale);
  CallibratedPostprocess->SetScalarParameterValue(TEXT("BlackDistance"), BlackDistance);
  CallibratedPostprocess->SetScalarParameterValue(TEXT("Mode"), (float)RenderMode);
  CallibratedPostprocess->SetVectorParameterValue(TEXT("BinScale"), (FLinearColor)BinScale);
  UE_LOG(LogTemp, Warning, TEXT("L:(%d,%d,%d) - E:(%d,%d,%d)"), SpaceOrigin.X, SpaceOrigin.Y, SpaceOrigin.Z, SpaceExtend.X, SpaceExtend.Y, SpaceExtend.Z);
  NextLocation = UKismetMathLibrary::RandomPointInBoundingBox(Flyspace->GetComponentLocation(), Flyspace->GetScaledBoxExtent());
  FrameCaptureCounter = FrameCaptureTime;

  SceneCam->PostProcessSettings.AutoExposureBias = this->AutoExposureBias;
  if (Rain)
  {
    // TODO do not assume that the only NiagaraActor is the rain
    ANiagaraActor* RainActor = Cast<ANiagaraActor>(UGameplayStatics::GetActorOfClass(world, ANiagaraActor::StaticClass()));
    if (RainActor)
    {
      RainActor->GetNiagaraComponent()->Activate(true);
      //RainActor->GetNiagaraComponent()->SetIntParameter(TEXT("SpawnRate"),RainParticlesPerSecond);
      RainActor->GetNiagaraComponent()->SetNiagaraVariableInt(TEXT("SpawnRate"), RainParticlesPerSecond);
    }
  }

  TArray<AActor*> Found;
  TArray<USceneComponent*> PlantInstances;
  UGameplayStatics::GetAllActorsOfClass(world, APawn::StaticClass(), Found);
  CollisionFilter.AddIgnoredActors(Found);
  Found.Empty();

  auto* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(GetWorld(),
    ADirectionalLight::StaticClass()));
  auto* Ambient = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(GetWorld(),
    ASkyLight::StaticClass()));
  auto* Clouds = Cast<AVolumetricCloud>(UGameplayStatics::GetActorOfClass(GetWorld(),
    AVolumetricCloud::StaticClass()));
  auto* Atmosphere = Cast<ASkyAtmosphere>(UGameplayStatics::GetActorOfClass(GetWorld(),
    ASkyAtmosphere::StaticClass()));

  if (Sun)
  {
    Sun->GetLightComponent()->SetIntensity(DirectionalIntensity);
    Sun->GetLightComponent()->SetIndirectLightingIntensity(DirectionalIndirectIntensity);

  }
  if (Ambient)
  {
    Ambient->GetLightComponent()->SetIntensity(AmbientIntensity);
    Ambient->GetLightComponent()->SetVolumetricScatteringIntensity(AmbientVolumeticScattering);
  }

  UGameplayStatics::GetAllActorsOfClass(world, AInstancedFoliageActor::StaticClass(), Found);
  CollisionFilter.AddIgnoredActors(Found);
  for (auto* FoilageActor : Found)
  {
    CollisionFilter.AddIgnoredActor(FoilageActor);
    FoilageActor->GetRootComponent()->GetChildrenComponents(false, PlantInstances);
    for (auto* PlantInstance : PlantInstances)
    {
      UInstancedStaticMeshComponent* RenderMesh = Cast<UInstancedStaticMeshComponent>(PlantInstance);
      if (RenderMesh)
      {
        RenderMesh->SetRenderCustomDepth(true);
        RenderMesh->SetCustomDepthStencilValue(1);
        RenderMesh->SetForcedLodModel(0);
      }
    }
  }
  if (DistanceToLandscape > 0.f)
  {
    auto* landscape = UGameplayStatics::GetActorOfClass(world, ALandscape::StaticClass());
    if (landscape)
    {
      FVector origin, extend;
      landscape->GetActorBounds(true, origin, extend, true);
      LowestLandscapeBound = origin.Z - (extend.Z + 100.f);
      EnsureDistancePreservation();
    }
    else
    {
      DistanceToLandscape = -1.f;
    }
  }
  this->SetActorTickEnabled(true);

  FQuat q1, q2;
  auto concatenate_transformat = q1 * q2;
  auto reverse_second_transform = q1 * (q2.Inverse());
  auto lerp_const_ang_velo = FQuat::Slerp(q1, q2, 0.5f);

  if (AdjustFocalDistance)
  {
    SceneCam->PostProcessSettings.DepthOfFieldBladeCount = 5;
    SceneCam->PostProcessSettings.DepthOfFieldDepthBlurAmount = 5.0f;
    SceneCam->PostProcessSettings.DepthOfFieldDepthBlurRadius = 2.0f;
    SceneCam->PostProcessSettings.DepthOfFieldFstop = 5.0f;
    SceneCam->PostProcessSettings.DepthOfFieldMinFstop = 2.0f;
    InfoCam->PostProcessSettings.DepthOfFieldBladeCount = 5;
    InfoCam->PostProcessSettings.DepthOfFieldDepthBlurAmount = 5.0f;
    InfoCam->PostProcessSettings.DepthOfFieldDepthBlurRadius = 2.0f;
    InfoCam->PostProcessSettings.DepthOfFieldFstop = 5.0f;
    InfoCam->PostProcessSettings.DepthOfFieldMinFstop = 2.0f;
  }

  this->WorldSpawner = Cast<AWorldSpawner>(UGameplayStatics::GetActorOfClass(world, AWorldSpawner::StaticClass()));
  if (WorldSpawner)
  {
    WorldSpawner->ReceiveStreamingCommunicatorRef(this);
  }
}

void ASynavisDrone::PostInitializeComponents()
{
  Super::PostInitializeComponents();
  auto* MatInst = UMaterialInstanceDynamic::Create(PostProcessMat, InfoCam);
  InfoCam->AddOrUpdateBlendable(MatInst);
}

EDataTypeIndicator ASynavisDrone::FindType(FProperty* Property)
{
  if (!Property)
  {
    return EDataTypeIndicator::Transform;
  }
  if (Property->IsA(FFloatProperty::StaticClass()))
  {
    return EDataTypeIndicator::Float;
  }
  else if (Property->IsA(FIntProperty::StaticClass()))
  {
    return EDataTypeIndicator::Int;
  }
  else if (Property->IsA(FBoolProperty::StaticClass()))
  {
    return EDataTypeIndicator::Bool;
  }
  else if (Property->IsA(FStrProperty::StaticClass()))
  {
    return EDataTypeIndicator::String;
  }
  else if (Property->IsA(FStructProperty::StaticClass()))
  {
    if (Property->ContainerPtrToValuePtr<FVector>(this))
    {
      return EDataTypeIndicator::Vector;
    }
    else if (Property->ContainerPtrToValuePtr<FRotator>(this))
    {
      return EDataTypeIndicator::Rotator;
    }
    else if (Property->ContainerPtrToValuePtr<FTransform>(this))
    {
      return EDataTypeIndicator::Transform;
    }
  }
  return EDataTypeIndicator::None;
}

void ASynavisDrone::EnsureDistancePreservation()
{
  TArray<FHitResult> MHits;
  if (GetWorld()->LineTraceMultiByObjectType(MHits, GetActorLocation(), FVector(GetActorLocation().X, GetActorLocation().Y, LowestLandscapeBound), ActorFilter, CollisionFilter))
  {
    for (auto Hit : MHits)
    {
      if (Hit.GetActor()->GetClass() == ALandscape::StaticClass())
      {
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Hit.ImpactPoint.Z + DistanceToLandscape));
      }
    }
  }
}

void ASynavisDrone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
}

// Called every frame
void ASynavisDrone::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  UGameplayStatics::GetPlayerPawn(GetWorld(), 0)->SetActorLocation(GetActorLocation());
  FrameCaptureCounter -= DeltaTime;
  FVector Distance = NextLocation - GetActorLocation();
  if (DistanceToLandscape > 0.f)
  {
    Distance.Z = 0;
  }
  if (FGenericPlatformMath::Abs((Distance).Size()) < 50.f)
  {
    NextLocation = UKismetMathLibrary::RandomPointInBoundingBox(Flyspace->GetComponentLocation(), Flyspace->GetScaledBoxExtent());
    if (GEngine && PrintScreenNewPosition)
    {
      GEngine->AddOnScreenDebugMessage(10, 30.f, FColor::Red, FString::Printf(
        TEXT("L:(%d,%d,%d) - N:(%d,%d,%d) - M:%d/%d"), \
        GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, \
        NextLocation.X, NextLocation.Y, NextLocation.Z, MeanVelocityLength, FGenericPlatformMath::Abs((GetActorLocation() - NextLocation).Size())));
    }
  }
  else
  {
    xprogress += DeltaTime;
    if (xprogress > 10000.f)
      xprogress = 0;
    FVector Noise = { FGenericPlatformMath::Sin(xprogress * CircleSpeed),FGenericPlatformMath::Cos(xprogress * CircleSpeed),-FGenericPlatformMath::Sin(xprogress * CircleSpeed) };
    Noise = (Noise / Noise.Size()) * CircleStrength;
    Distance = Distance / Distance.Size();
    Velocity = (Velocity * TurnWeight) + (Distance * (1.f - TurnWeight)) + Noise;
    Velocity = Velocity / Velocity.Size();
    SetActorLocation(GetActorLocation() + (Velocity * DeltaTime * MaxVelocity));
    if (!EditorOrientedCamera)
      SetActorRotation(Velocity.ToOrientationRotator());
    if (DistanceToLandscape > 0.f)
    {
      EnsureDistancePreservation();
    }
  }
  FHitResult Hit;
  if (GetWorld()->LineTraceSingleByObjectType(Hit, GetActorLocation(), GetActorLocation() + GetActorForwardVector() * 2000.f, ParamsObject, ParamsTrace))
  {
    TargetFocalLength = FGenericPlatformMath::Min(2000.f, FGenericPlatformMath::Max(0.f, Hit.Distance));
  }
  else
  {
    TargetFocalLength = 2000.f;
  }
  if (FGenericPlatformMath::Abs(TargetFocalLength - FocalLength) > 0.1f)
  {
    FocalLength = (TargetFocalLength - FocalLength) * FocalRate * DeltaTime;
    SceneCam->PostProcessSettings.DepthOfFieldFocalDistance = FocalLength;
    InfoCam->PostProcessSettings.DepthOfFieldFocalDistance = FocalLength;
  }
  if (TransmissionTargets.Num() > 0)
  {
    FString Data = TEXT("{\"type\":\"track\",\"data\":{");

    for (auto i = 0; i < TransmissionTargets.Num(); ++i)
    {
      auto& Target = TransmissionTargets[i];
      if (IsValid(Target.Object))
      {
        Data.Append(FString::Printf(TEXT("\"%s\":"), *Target.Name));
        // read the property
        switch (Target.DataType)
        {
        case EDataTypeIndicator::Float:
          Data.Append(FString::Printf(TEXT("%f"), Target.Property->ContainerPtrToValuePtr<float>(Target.Object)));
          break;
        case EDataTypeIndicator::Int:
          Data.Append(FString::Printf(TEXT("%d"), Target.Property->ContainerPtrToValuePtr<int>(Target.Object)));
          break;
        case EDataTypeIndicator::Bool:
          Data.Append(FString::Printf(TEXT("%s"), Target.Property->ContainerPtrToValuePtr<bool>(Target.Object) ? TEXT("true") : TEXT("false")));
          break;
        case EDataTypeIndicator::String:
          Data.Append(FString::Printf(TEXT("\"%s\""), **Target.Property->ContainerPtrToValuePtr<FString>(Target.Object)));
          break;
        case EDataTypeIndicator::Transform:
          Data.Append(PrintFormattedTransform(Target.Object));
          break;
        case EDataTypeIndicator::Vector:
          Data.Append(FString::Printf(TEXT("[%f,%f,%f]"), Target.Property->ContainerPtrToValuePtr<FVector>(Target.Object)->X, Target.Property->ContainerPtrToValuePtr<FVector>(Target.Object)->Y, Target.Property->ContainerPtrToValuePtr<FVector>(Target.Object)->Z));
          break;
        case EDataTypeIndicator::Rotator:
          Data.Append(FString::Printf(TEXT("[%f,%f,%f]"), Target.Property->ContainerPtrToValuePtr<FRotator>(Target.Object)->Pitch, Target.Property->ContainerPtrToValuePtr<FRotator>(Target.Object)->Yaw, Target.Property->ContainerPtrToValuePtr<FRotator>(Target.Object)->Roll));
          break;
        }
        if (i < TransmissionTargets.Num() - 1)
          Data.Append(TEXT(","));
      }
    }
    Data.Append(TEXT("}}"));
    SendResponse(Data);
  }

  // prepare texture for storage
  if (FrameCaptureTime > 0.f && FrameCaptureCounter <= 0.f)
  {
    auto* irtarget = InfoCam->TextureTarget->GameThread_GetRenderTargetResource();
    auto* srtarget = SceneCam->TextureTarget->GameThread_GetRenderTargetResource();

    TPair<TArray<FColor>, TArray<FColor>> Data;

    FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
    ReadPixelFlags.SetLinearToGamma(true);
    srtarget->ReadPixels(Data.Value, ReadPixelFlags);
    irtarget->ReadPixels(Data.Key, ReadPixelFlags);

    FBase64 Base64;

    TArray<uint8> DataAsBytes(reinterpret_cast<uint8*>(Data.Key.GetData()), Data.Key.Num() * sizeof(FColor));

    FString Base64String = Base64.Encode(DataAsBytes);



    FrameCaptureCounter = FrameCaptureTime;
    //UE_LOG(LogTemp, Display, TEXT("Setting Frame back to %f"),FrameCaptureTime);
  }

}
