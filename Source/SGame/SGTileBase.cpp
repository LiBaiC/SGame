// Fill out your copyright notice in the Description page of Project Settings.

#include "SGame.h"
#include "SGTileBase.h"

// Sets default values
ASGTileBase::ASGTileBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// We want the tile can be moved (falling), so we need a root component
	SetRootComponent(GetRenderComponent());
}

// Called when the game starts or when spawned
void ASGTileBase::BeginPlay()
{
	Super::BeginPlay();

	// Set our class up to handle clicks and touches.
	OnClicked.AddUniqueDynamic(this, &ASGTileBase::TilePress_Mouse);
	OnBeginCursorOver.AddUniqueDynamic(this, &ASGTileBase::TileEnter_Mouse);

	OnInputTouchBegin.AddUniqueDynamic(this, &ASGTileBase::TilePress);
	OnInputTouchEnter.AddUniqueDynamic(this, &ASGTileBase::TileEnter);

	FString EndPointName = FString::Printf(TEXT("Gameplay_Tile_%d"), GridAddress);
	MessageEndpoint = FMessageEndpoint::Builder(*EndPointName)
		.Handling<FMessage_Gameplay_TileSelectableStatusChange>(this, &ASGTileBase::HandleSelectableStatusChange)
		.Handling<FMessage_Gameplay_TileLinkedStatusChange>(this, &ASGTileBase::HandleLinkStatusChange);
	if (MessageEndpoint.IsValid() == true)
	{
		// Subscribe the tile need events
		MessageEndpoint->Subscribe<FMessage_Gameplay_TileSelectableStatusChange>();
		MessageEndpoint->Subscribe<FMessage_Gameplay_TileLinkedStatusChange>();
	}
}

// Called every frame
void ASGTileBase::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

void ASGTileBase::TilePress(ETouchIndex::Type FingerIndex)
{
	UE_LOG(LogSGame, Log, TEXT("Tile %s was pressed, address (%d,%d)"), *GetName(), GridAddress % 6, GridAddress / 6);

	// Tell the game logic, the new tile is picked
	FMessage_Gameplay_NewTilePicked* TilePickedMessage = new FMessage_Gameplay_NewTilePicked();
	TilePickedMessage->TileAddress = GridAddress;
	if (MessageEndpoint.IsValid() == true)
	{
		MessageEndpoint->Publish(TilePickedMessage, EMessageScope::Process);
	}
}

void ASGTileBase::TileEnter(ETouchIndex::Type FingerIndex)
{
	UE_LOG(LogSGame, Log, TEXT("Tile %s was entered, address (%d,%d)"), *GetName(), GridAddress % 6, GridAddress / 6);
}

void ASGTileBase::TilePress_Mouse()
{
	TilePress(ETouchIndex::Touch1);
}

void ASGTileBase::TileEnter_Mouse()
{
	// This is meant to simulate finger-swiping, so ignore if the mouse isn't clicked.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			TileEnter(ETouchIndex::Touch1);
		}
	}
}

bool ASGTileBase::IsSelectable() const
{
	return TileData.TileStatusArray.Contains(ESGTileStatusFlag::ESF_SELECTABLE);
}

void ASGTileBase::SetGridAddress(int32 NewLocation)
{
	GridAddress = NewLocation;
}

int32 ASGTileBase::GetGridAddress() const
{
	return GridAddress;
}

void ASGTileBase::HandleSelectableStatusChange(const FMessage_Gameplay_TileSelectableStatusChange& Message, const IMessageContextRef& Context)
{
	if (Message.TileAddress != -1 && Message.TileAddress != GridAddress)
	{
		// Selectable event is not sent to this tile
		return;
	}

	UE_LOG(LogSGame, Log, TEXT("Tile %d selectable flag changed to %d"), GridAddress, Message.NewSelectableStatus);

	if (Message.NewSelectableStatus == true)
	{
		// Add the selectable flag to the status array
		TileData.TileStatusArray.AddUnique(ESGTileStatusFlag::ESF_SELECTABLE);

		// Set the white color 
		GetRenderComponent()->SetSpriteColor(FLinearColor::White);
	}
	else
	{
		// Remove the selectable flag
		TileData.TileStatusArray.Remove(ESGTileStatusFlag::ESF_SELECTABLE);

		// Dim the sprite
		GetRenderComponent()->SetSpriteColor(FLinearColor(0.2f, 0.2f, 0.2f));
	}
}

void ASGTileBase::HandleLinkStatusChange(const FMessage_Gameplay_TileLinkedStatusChange& Message, const IMessageContextRef& Context)
{
	if (Message.TileAddress != -1 && Message.TileAddress != GridAddress)
	{
		// Selectable event is not sent to this tile
		return;
	}

	UE_LOG(LogSGame, Log, TEXT("Tile %d link status changed to %d"), GridAddress, Message.NewLinkStatus);

	if (Message.NewLinkStatus == true)
	{
		// Add the selectable flag to the status array
		TileData.TileStatusArray.AddUnique(ESGTileStatusFlag::ESF_LINKED);

		// Set the linked sprite
		GetRenderComponent()->SetSprite(Sprite_Selected);
	}
	else
	{
		// Remove the selectable flag
		TileData.TileStatusArray.Remove(ESGTileStatusFlag::ESF_LINKED);

		// Set the normal sprite
		GetRenderComponent()->SetSprite(Sprite_Normal);
	}
}

