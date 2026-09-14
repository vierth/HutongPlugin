#include "PlaceLabelSubsystem.h"

#include "PlaceLabelHUDWidget.h"
#include "PlaceLabelSettings.h"
#include "PlaceRegionComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UPlaceLabelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// UTickableWorldSubsystem only starts ticking if subclasses forward this.
	Super::Initialize(Collection);
}

void UPlaceLabelSubsystem::Deinitialize()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	Regions.Reset();
	CurrentChain.Reset();

	Super::Deinitialize();
}

void UPlaceLabelSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	bHUDCreationPending = Settings && Settings->bAutoCreateHUDWidget;
	TryCreateHUD();
}

void UPlaceLabelSubsystem::RegisterRegion(UPlaceRegionComponent* Region)
{
	if (Region)
	{
		Regions.AddUnique(Region);
	}
}

void UPlaceLabelSubsystem::UnregisterRegion(UPlaceRegionComponent* Region)
{
	Regions.Remove(Region);

	// A region leaving while the player is inside it would otherwise leave a stale chain on screen pointing at a dead component.
	for (const FPlaceLabelEntry& Entry : CurrentChain)
	{
		if (Entry.Region.Get() == Region)
		{
			CurrentChain.Reset();
			OnChainChanged.Broadcast(CurrentChain);
			if (HUDWidget)
			{
				HUDWidget->SetChain(CurrentChain);
			}
			break;
		}
	}
}

void UPlaceLabelSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bHUDCreationPending)
	{
		TryCreateHUD();
	}

	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	const float Interval = Settings ? FMath::Max(Settings->QueryInterval, 0.0f) : 0.2f;

	Accumulator += DeltaTime;
	if (Accumulator < Interval)
	{
		return;
	}
	Accumulator = 0.0f;

	FVector PlayerLocation;
	if (TryGetPlayerLocation(PlayerLocation))
	{
		UpdateForLocation(PlayerLocation);
	}
}

bool UPlaceLabelSubsystem::TryGetPlayerLocation(FVector& OutLocation) const
{
	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	const int32 PlayerIndex = Settings ? Settings->PlayerIndex : 0;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), PlayerIndex);
	if (!PC || !PC->IsLocalController())
	{
		return false;
	}

	if (const APawn* Pawn = PC->GetPawn())
	{
		OutLocation = Pawn->GetActorLocation();
		return true;
	}

	// Spectating or between possessions: the camera is still a meaningful "where the player is".
	if (PC->PlayerCameraManager)
	{
		OutLocation = PC->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	return false;
}

void UPlaceLabelSubsystem::UpdateForLocation(const FVector& WorldLocation, bool bForce)
{
	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	const float MinMove = Settings ? FMath::Max(Settings->MinMoveToRequery, 0.0f) : 50.0f;

	if (!bForce && bHasQueried
		&& FVector::DistSquared2D(WorldLocation, LastQueryLocation) < FMath::Square(MinMove))
	{
		return;
	}
	LastQueryLocation = WorldLocation;
	bHasQueried = true;

	UPlaceRegionComponent* Innermost = FindInnermostRegion(WorldLocation);

	TArray<FPlaceLabelEntry> NewChain;
	BuildChain(Innermost, NewChain);

	if (!ChainDiffers(NewChain))
	{
		return;
	}

	CurrentChain = MoveTemp(NewChain);
	OnChainChanged.Broadcast(CurrentChain);
	if (HUDWidget)
	{
		HUDWidget->SetChain(CurrentChain);
	}
}

UPlaceRegionComponent* UPlaceLabelSubsystem::FindInnermostRegion(const FVector& WorldLocation) const
{
	UPlaceRegionComponent* Best = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Lowest();
	double BestArea = TNumericLimits<double>::Max();

	// Brute force with a bounds reject inside ContainsWorldPoint.
	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : Regions)
	{
		UPlaceRegionComponent* Region = Weak.Get();
		if (!Region || !Region->ContainsWorldPoint(WorldLocation))
		{
			continue;
		}

		const int32 Priority = Region->GetDisplayPriority();
		const double Area = Region->GetWorldArea();

		// An untyped region scores 0 and therefore loses to any typed one.
		if (Priority > BestPriority || (Priority == BestPriority && Area < BestArea))
		{
			Best = Region;
			BestPriority = Priority;
			BestArea = Area;
		}
	}
	return Best;
}

void UPlaceLabelSubsystem::BuildChain(
	UPlaceRegionComponent* Innermost, TArray<FPlaceLabelEntry>& OutChain) const
{
	OutChain.Reset();
	if (!Innermost)
	{
		return;
	}

	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	const int32 MaxDepth = Settings ? FMath::Max(Settings->MaxChainDepth, 1) : 16;

	// Both a visited set and a depth cap.
	TSet<const UPlaceRegionComponent*> Visited;
	UPlaceRegionComponent* Current = Innermost;
	int32 Depth = 0;

	while (Current && Depth < MaxDepth && !Visited.Contains(Current))
	{
		Visited.Add(Current);

		FPlaceLabelEntry& Entry = OutChain.AddDefaulted_GetRef();
		Entry.Name = Current->Name;
		Entry.Type = Current->Type;
		Entry.TypeId = Current->GetTypeId();
		Entry.Note = Current->Note;
		Entry.Depth = Depth;
		Entry.Region = Current;

		++Depth;
		Current = Current->GetEffectiveParent();
	}

	if (Current)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PlaceLabels: parent chain from '%s' hit a cycle or the depth cap of %d."),
			*Innermost->GetPathName(), MaxDepth);
	}
}

bool UPlaceLabelSubsystem::ChainDiffers(const TArray<FPlaceLabelEntry>& NewChain) const
{
	if (NewChain.Num() != CurrentChain.Num())
	{
		return true;
	}
	for (int32 i = 0; i < NewChain.Num(); ++i)
	{
		if (NewChain[i].Region != CurrentChain[i].Region)
		{
			return true;
		}
	}
	return false;
}

void UPlaceLabelSubsystem::TryCreateHUD()
{
	if (HUDWidget)
	{
		bHUDCreationPending = false;
		return;
	}

	const UPlaceLabelSettings* Settings = GetDefault<UPlaceLabelSettings>();
	if (!Settings || !Settings->bAutoCreateHUDWidget)
	{
		bHUDCreationPending = false;
		return;
	}

	UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_DedicatedServer)
	{
		// Chain resolution stays useful server-side; only the widget is pointless there.
		bHUDCreationPending = false;
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, Settings->PlayerIndex);
	if (!PC || !PC->IsLocalController())
	{
		// Retry next tick — the local controller commonly does not exist yet at world BeginPlay.
		return;
	}

	if (Settings->HUDWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PlaceLabels: Auto Create HUD Widget is on but no HUD Widget Class is set "
				 "(Project Settings > Plugins > Place Labels)."));
		bHUDCreationPending = false;
		return;
	}

	// Synchronous is fine here: once, at level start, off any hot path.
	UClass* WidgetClass = Settings->HUDWidgetClass.LoadSynchronous();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceLabels: could not load HUD widget class '%s'."),
			*Settings->HUDWidgetClass.ToString());
		bHUDCreationPending = false;
		return;
	}

	HUDWidget = CreateWidget<UPlaceLabelHUDWidget>(PC, WidgetClass);
	if (HUDWidget)
	{
		// AddToPlayerScreen rather than AddToViewport.
		HUDWidget->AddToPlayerScreen(Settings->HUDZOrder);
		HUDWidget->SetChain(CurrentChain);
	}
	bHUDCreationPending = false;
}
