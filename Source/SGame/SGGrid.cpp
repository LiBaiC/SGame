// Fill out your copyright notice in the Description page of Project Settings.

#include "SGame.h"
#include "Math/UnrealMathUtility.h"

#include "SGGrid.h"

// Sets default values
ASGGrid::ASGGrid(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	TileManager = CreateDefaultSubobject<USGTileManager>(TEXT("TileManager"));

	TileSize.Set(106.67f, 106.67f);
}

// Called when the game starts or when spawned
void ASGGrid::BeginPlay()
{
	Super::BeginPlay();
	
	MessageEndpoint = FMessageEndpoint::Builder("Gameplay_Grid")
		.Handling<FMessage_Gameplay_TileDisappear>(this, &ASGGrid::HandleTileDisappear);
	if (MessageEndpoint.IsValid() == true)
	{
		// Subscribe the grid needed messages
		MessageEndpoint->Subscribe<FMessage_Gameplay_TileDisappear>();
	}

	// Initialize the grid
	GridTiles.Empty(GridWidth * GridHeight);
	GridTiles.AddZeroed(GridWidth * GridHeight);
}

// Called every frame
void ASGGrid::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
}

void ASGGrid::Condense()
{
	TMap<int32, int32> GridHoleNumMap;

	// Iterate the each colum of grid tiles arry, find the holes
	for (int columnIndex = 0; columnIndex < GridWidth; columnIndex++)
	{
		for (int rowIndex = 0; rowIndex < GridHeight; rowIndex++)
		{
			// If it is hole already, pass it
			int gridAddress = GridCorrdsToAddress(columnIndex, rowIndex);
			if (GridTiles[gridAddress] == nullptr)
			{
				continue;
			}

			int currentGridHoleNum = 0;
			// Search below, to find if there is any NULL tiles
			for (int j = rowIndex + 1; j < GridHeight; j++)
			{
				int testAddress = GridCorrdsToAddress(columnIndex, j);
				if (GridTiles[testAddress] == nullptr)
				{
					currentGridHoleNum++;
				}
			}

			if (currentGridHoleNum > 0)
			{
				// Insert it into the map
				GridHoleNumMap.Add(GridCorrdsToAddress(columnIndex, rowIndex), currentGridHoleNum);

				UE_LOG(LogSGame, Log, TEXT("Tile Address: %d will move down %d"), GridCorrdsToAddress(columnIndex, rowIndex), currentGridHoleNum);
			}
		}
	}

	// Iterate the each column of grid tiles arry, move down the tiles
	for (int columnIndex = 0; columnIndex < GridWidth; columnIndex++)
	{
		// From down to up
		for (int rowIndex = GridHeight - 1; rowIndex >= 0; rowIndex--)
		{
			int testAddress = GridCorrdsToAddress(columnIndex, rowIndex);
			if (GridHoleNumMap.Find(testAddress) == nullptr)
			{
				// No holes below, continue,
				continue;
			}

			int MoveDownNum = GridHoleNumMap[testAddress];

			// Send tile move message to the tile
			const ASGTileBase* testTile = GridTiles[testAddress];
			checkSlow(testTile != nullptr);

			// Update the tile new address
			FMessage_Gameplay_TileMoved* TileMoveMessage = new FMessage_Gameplay_TileMoved();
			TileMoveMessage->TileID = testTile->GetTileID();
			TileMoveMessage->OldTileAddress = testTile->GetGridAddress();
			TileMoveMessage->NewTileAddress = GridCorrdsToAddress(columnIndex, rowIndex + MoveDownNum);

			// Publish the message
			if (MessageEndpoint.IsValid() == true)
			{
				MessageEndpoint->Publish(TileMoveMessage, EMessageScope::Process);
			}
			
			// Set the tile to the new tile address
			GridTiles[TileMoveMessage->NewTileAddress] = testTile;
		}
	}
}

void ASGGrid::RefillGrid()
{
	for (int32 Col = 0; Col < GridWidth; ++Col)
	{
		const ASGTileBase* CurrentColumnTopTile = GridTiles[Col];
		if (CurrentColumnTopTile != nullptr)
		{
			// There is tile on top row, so no need to refill
			continue;
		}

		// Find how many empty space we have
		int32 RowNum = 0;
		while (RowNum < GridHeight)
		{
			int32 NewGridAddress = Col + RowNum * GridWidth;
			const ASGTileBase* CurrentTile = GetTileFromGridAddress(NewGridAddress);
			if (CurrentTile == nullptr)
			{
				++RowNum;
			}
			else
			{
				break;
			}
		}

		RefillColumn(Col, RowNum);
	}
}

void ASGGrid::RefillColumn(int32 inColumnIndex, int32 inNum)
{
	// We always start fill the first row [0 index row]
	for (int startRow = 0; startRow < inNum; startRow++)
	{
		// Calculate the new grid address
		int32 TileID = TileManager->SelectTileFromLibrary();
		int32 GridAddress;
		FVector SpawnLocation;
		GetGridAddressWithOffset(0, inColumnIndex, startRow, GridAddress);
		SpawnLocation = GetLocationFromGridAddress(GridAddress);

		// create the tile at the specified location
		ASGTileBase* NewTile = TileManager->CreateTile(this, SpawnLocation, GridAddress, TileID);
		if (NewTile == nullptr)
		{
			UE_LOG(LogSGame, Error, TEXT("Cannot create tile at location %d, %d"), inColumnIndex, startRow);
		}

		// Refill the grid with tile
		RefillGridAddressWithTile(GridAddress, NewTile);
	}
}

void ASGGrid::RefillGridAddressWithTile(int32 inGridAddress, ASGTileBase* inTile)
{
	checkSlow(GridTiles.IsValidIndex(inGridAddress));
	checkSlow(inTile != nullptr);

	GridTiles[inGridAddress] = inTile;
}

const ASGTileBase* ASGGrid::GetTileFromGridAddress(int32 GridAddress)
{
	if (GridAddress < GridTiles.Num())
	{
		return GridTiles[GridAddress];
	}
	else
	{
		UE_LOG(LogSGame, Log, TEXT("Invalid grid address, will return null tile"));
		return nullptr;
	}
}

const ASGTileBase* ASGGrid::GetTileFromTileID(int32 inTileID)
{
	for (int i = 0; i < GridTiles.Num(); i++)
	{
		if (GridTiles[i] != nullptr && GridTiles[i]->GetTileID() == inTileID)
		{
			return GridTiles[i];
		}
	}

	return nullptr;
}

FVector ASGGrid::GetLocationFromGridAddress(int32 GridAddress)
{
	checkSlow(TileSize.X > 0.0f);
	checkSlow(TileSize.Y > 0.0f);
	checkSlow(GridWidth > 0);
	checkSlow(GridHeight > 0);
	FVector Center = GetActorLocation();
	FVector OutLocation = FVector(-(GridWidth / 2.0f) * TileSize.X + (TileSize.X * 0.5f), 0.0f, -(GridHeight / 2.0f) * TileSize.Y + (TileSize.Y * 0.5f));
	OutLocation.X += TileSize.X * (float)(GridAddress % GridWidth);
	OutLocation.Z += TileSize.Y * (float)(GridAddress / GridWidth);
	OutLocation += Center;

	return OutLocation;
}

bool ASGGrid::GetGridAddressWithOffset(int32 InitialGridAddress, int32 XOffset, int32 YOffset, int32 &ReturnGridAddress)
{
	checkSlow(TileSize.X > 0.0f);
	checkSlow(TileSize.Y > 0.0f);
	checkSlow(GridWidth > 0);
	checkSlow(GridHeight > 0);
	int32 NewAxisValue;

	// Initialize to an invalid address.
	ReturnGridAddress = -1;

	// Check for going off the map in the X direction.
	NewAxisValue = (InitialGridAddress % GridWidth) + XOffset;
	if (NewAxisValue != FMath::Clamp(NewAxisValue, 0, (GridWidth - 1)))
	{
		return false;
	}

	// Check for going off the map in the Y direction.
	NewAxisValue = (InitialGridAddress / GridWidth) + YOffset;
	if (NewAxisValue != FMath::Clamp(NewAxisValue, 0, (GridHeight - 1)))
	{
		return false;
	}

	ReturnGridAddress = (InitialGridAddress + XOffset + (YOffset * GridWidth));
	checkSlow(ReturnGridAddress >= 0);
	checkSlow(ReturnGridAddress < (GridWidth * GridHeight));
	return true;
}

bool ASGGrid::AreAddressesNeighbors(int32 GridAddressA, int32 GridAddressB)
{
	if (GridAddressA == GridAddressB)
	{
		UE_LOG(LogSGame, Warning, TEXT("Pass in the same addresses"));
		return false;
	}
	else if ((FMath::Min(GridAddressA, GridAddressB) < 0) || (FMath::Max(GridAddressA, GridAddressB) >= (GridWidth * GridHeight)))
	{
		UE_LOG(LogSGame, Warning, TEXT("Pass in the invalid addresses"));
		return false;
	}

	auto AddressARow = GridAddressA / GridWidth;
	auto AddressAColumn = GridAddressA % GridWidth;
	auto AddressBRow = GridAddressB / GridWidth;
	auto AddressBColumn = GridAddressB % GridWidth;

	// The two address are neighbors only if there row and column distance less than 1
	if (FMath::Abs(AddressARow - AddressBRow) <= 1 && FMath::Abs(AddressAColumn - AddressBColumn) <= 1)
	{
		return true;
	}

	return false;
}

void ASGGrid::HandleTileDisappear(const FMessage_Gameplay_TileDisappear& Message, const IMessageContextRef& Context)
{
	for (int i = 0; i < Message.TilesAddressToDisappear.Num(); i++)
	{
		int32 disappearTileAddress = Message.TilesAddressToDisappear[i];
		checkSlow(GridTiles[disappearTileAddress] != nullptr);

		// Tell the tiles, it was collected

		// Set null to the grid tiles array
		GridTiles[disappearTileAddress] = nullptr;
	}

	// Conden the grid
	Condense();
}
