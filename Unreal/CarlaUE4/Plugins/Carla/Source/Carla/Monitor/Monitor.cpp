// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "Monitor.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"
#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

// Carla
#include "Vehicle/WheeledVehicleAIController.h"


// Clingo staticly linked library
THIRD_PARTY_INCLUDES_START
#pragma push_macro("check")
#undef check
#include <clingo.hh>
#pragma pop_macro("check")
THIRD_PARTY_INCLUDES_END


// Developer
#include "Fork.h"


// STL
#include <fstream>
#include <sstream>


// Sets default values
AMonitor::AMonitor(const FObjectInitializer &ObjectInitializer)
	:Super(ObjectInitializer)
{
	RootComponent =
		ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneRootComponent"));
	RootComponent->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UTexture2D> MonitorBillboardAsset(TEXT("Texture2D'/Game/Monitor/Monitor.Monitor'"));
	Billboard = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Billboard"), true);
	if (MonitorBillboardAsset.Object != nullptr && Billboard != nullptr)
	{
		Billboard->SetSprite(MonitorBillboardAsset.Object);
		Billboard->SetupAttachment(RootComponent);
		Billboard->SetRelativeLocation(FVector{ 0.f, 0.f, 300.f });
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to instantiate billboard or its sprite!"));
	}

	ExtentBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	ExtentBox->SetupAttachment(RootComponent);
	ExtentBox->SetHiddenInGame(true);
	ExtentBox->SetMobility(EComponentMobility::Static);
	ExtentBox->SetCollisionProfileName(FName("OverlapAll"));
	ExtentBox->SetGenerateOverlapEvents(true);
	ExtentBox->SetBoxExtent(FVector{ 1800.0f, 1800.0f, 100.0f });
	ExtentBox->ShapeColor = FColor(255, 255, 255);

	SetActorHiddenInGame(true);
}


void AMonitor::OnConstruction(const FTransform &Transform)
{
	TArray<AExit*> MyExits;
	GetIntersectingActors<AExit>(MyExits);
	TArray<AFork*> MyForks;
	GetIntersectingActors<AFork>(MyForks);
	for (AFork* Fork : MyForks)
	{
		for (AExit* Exit : MyExits)
		{
			Fork->AddExit(Exit);
		}
	}
}


// Called when the game starts or when spawned
void AMonitor::BeginPlay()
{
	Super::BeginPlay();

	CreateLogFile();

	SetupTriggers();

	LoadGeometryFacts();
	WriteGeometryToFile();

	LoadTrafficRules();
}


void AMonitor::SetupTriggers()
{
	ExtentBox->OnComponentEndOverlap.AddDynamic(this, &AMonitor::OnExitMonitor);

	TArray<AActor *> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		ALane* Lane = Cast<ALane>(Actor);
		AFork* Fork = Cast<AFork>(Actor);
		if (Lane != nullptr)
		{
			Lane->OnActorBeginOverlap.AddDynamic(this, &AMonitor::OnEnterLane);
			Lane->OnActorEndOverlap.AddDynamic(this, &AMonitor::OnExitLane);
		}
		else if (Fork != nullptr)
		{
			Fork->ArrivalTriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AMonitor::OnArrival);
			Fork->EntranceTriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &AMonitor::OnEntrance);
		}
	}
}


void AMonitor::CreateLogFile()
{
	// Init logfile name and path
	LogFileName = GetName() + "Log.cl";
	LogFileFullName = FPaths::ProjectSavedDir() + LogFileName;

	// Create the logfile 
	std::ofstream LogFile(TCHAR_TO_UTF8(*LogFileFullName), std::ios::trunc);

	// Close the file stream explicitly
	LogFile.close();
}


void AMonitor::LoadGeometryFacts()
{
	// Get a list of Forks
	TArray<AActor *> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	TArray<AFork *> Forks;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		AFork* Fork = Cast<AFork>(OverlappingActor);
		if (Fork != nullptr)
		{
			Forks.Add(Fork);
		}
	}
	NumberOfForks = Forks.Num();

	// "isToTheRightOf()" facts
	for (size_t i = 0; i < NumberOfForks; i++)
	{
		for (size_t j = i + 1; j < NumberOfForks; j++)
		{
			FString LeftFork = "f_";
			FString RightFork = "f_";
			if (Forks[i]->IsOnRightOf(Forks[j])) // angle in (30, 150)
			{
				LeftFork += Forks[j]->GetName();
				RightFork += Forks[i]->GetName();
			}
			else if (Forks[j]->IsOnRightOf(Forks[i])) // angle in (-150, -30)
			{
				LeftFork += Forks[i]->GetName();
				RightFork += Forks[j]->GetName();
			}
			else
			{
				continue;
			}
			FString FactString = "isOnRightOf("
				+ RightFork + ", "
				+ LeftFork + ").";
			Geometry += TCHAR_TO_ANSI(*FactString);
			Geometry += "\n";
		}
	}

	// Get a list of Lanes
	TArray<ALane *> Lanes;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		ALane* Lane = Cast<ALane>(OverlappingActor);
		if (Lane != nullptr)
		{
			Lanes.Add(Lane);
		}
	}

	// Graph connectivity
	for (ALane* Lane : Lanes)
	{
		FString FactString = "laneFromTo(l_"
			+ Lane->GetName() + ", f_"
			+ Lane->MyFork->GetName() + ", e_"
			+ Lane->MyExit->GetName() + ").";
		Geometry += TCHAR_TO_ANSI(*FactString);
		Geometry += "\n";

		FactString = "laneCorrectSignal(l_"
			+ Lane->GetName() + ", "
			+ Lane->GetCorrectSignal() + ").";
		Geometry += TCHAR_TO_ANSI(*FactString);
		Geometry += "\n";
	}

	// Lane overlaps
	for (size_t i = 0; i < Lanes.Num(); i++)
	{
		FString FactString = "overlaps(l_"
			+ Lanes[i]->GetName() + ", l_"
			+ Lanes[i]->GetName() + ").";
		Geometry += TCHAR_TO_ANSI(*FactString);
		Geometry += "\n";
		for (size_t j = i + 1; j < Lanes.Num(); j++)
		{
			if (Lanes[i]->IsOverlappingActor(Lanes[j]))
			{
				FactString = "overlaps(l_"
					+ Lanes[i]->GetName() + ", l_"
					+ Lanes[j]->GetName() + ").";
				Geometry += TCHAR_TO_ANSI(*FactString);
				Geometry += "\n";
				FactString = "overlaps(l_"
					+ Lanes[j]->GetName() + ", l_"
					+ Lanes[i]->GetName() + ").";
				Geometry += TCHAR_TO_ANSI(*FactString);
				Geometry += "\n";
			}
		}
	}
}


void AMonitor::LoadTrafficRules()
{
	FString RulesFileFullName = FPaths::ProjectPluginsDir() + "Carla/Source/Carla/Monitor/all-way-stop.cl";
	std::ifstream RulesFile(TCHAR_TO_UTF8(*RulesFileFullName));
	std::stringstream buffer;
	buffer << RulesFile.rdbuf();
	TrafficRules = buffer.str();
	RulesFile.close();
}


void AMonitor::AddEvent(FString Actor, FString Atom)
{
	// TODO: Use TimeStep to buffer concurrent events
	FString& PreviousAtoms = ActorToEventsMap.FindOrAdd(Actor);
	PreviousAtoms += Atom + "\n";
	UE_LOG(LogTemp, Warning, TEXT("Event: %s"), *Atom);
}


void AMonitor::AppendToLogfile(std::string Content)
{
	std::ofstream LogFile(TCHAR_TO_UTF8(*LogFileFullName), std::ios::app);
	LogFile << Content << std::endl;
	LogFile.close();
}


void AMonitor::WriteGeometryToFile()
{
	FString GeometryFileFullName = FPaths::ProjectSavedDir() + GetName() + "Geometry.cl";
	std::ofstream GeometryFile(TCHAR_TO_UTF8(*GeometryFileFullName), std::ios::trunc);
	GeometryFile << Geometry;
	GeometryFile.close();
}

void AMonitor::OnArrival(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	int32 TimeStep = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / TimeResolution);
	FString ArrivingVehicleID = "v_" + OtherActor->GetName();
	FString Fork = "f_" + OverlappedComp->GetOwner()->GetName();
	FString EventAtom = "arrivesAtForkAtTime("
		+ ArrivingVehicleID + ", "
		+ Fork + ", "
		+ FString::FromInt(TimeStep) + ").";
	AddEvent(OtherActor->GetName(), EventAtom);

	ACarlaWheeledVehicle* ArrivingVehicle = Cast<ACarlaWheeledVehicle>(OtherActor);
	if (ArrivingVehicle != nullptr)
	{
		FString SignalString = SignalToString(ArrivingVehicle->GetSignalState());
		EventAtom = "signalsAtForkAtTime("
			+ ArrivingVehicleID + ", "
			+ SignalString + ", "
			+ Fork + ", "
			+ FString::FromInt(TimeStep) + ").";
		AddEvent(OtherActor->GetName(), EventAtom);
		VehiclePointers.Add(OtherActor->GetName(), ArrivingVehicle);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Cast to ACarlaWheeledVehicle failed!"));
	}

	Solve();
}


void AMonitor::OnEntrance(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	uint32 TimeStep = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / TimeResolution);
	FString EnteringVehicle = "v_" + OtherActor->GetName();
	FString Fork = "f_" + OverlappedComp->GetOwner()->GetName();
	FString Atom = "entersForkAtTime("
		+ EnteringVehicle + ", "
		+ Fork + ", "
		+ FString::FromInt(TimeStep) + ").";

	AddEvent(OtherActor->GetName(), Atom);
	Solve();
}


void AMonitor::OnEnterLane(AActor* ThisActor, AActor* OtherActor)
{
	int32 TimeStep = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / TimeResolution);
	FString EnteringActorName = "v_" + OtherActor->GetName();
	FString LaneName = "l_" + ThisActor->GetName();
	FString Atom = "entersLaneAtTime("
		+ EnteringActorName + ", "
		+ LaneName + ", "
		+ FString::FromInt(TimeStep) + ").";
	AddEvent(OtherActor->GetName(), Atom);
	Solve();
}


void AMonitor::OnExitLane(AActor* ThisActor, AActor* OtherActor)
{
	int32 TimeStep = FMath::FloorToInt(GetWorld()->GetTimeSeconds() / TimeResolution);
	FString ExitingActorName = "v_" + OtherActor->GetName();
	FString LaneName = "l_" + ThisActor->GetName();
	FString Atom = "leavesLaneAtTime("
		+ ExitingActorName + ", "
		+ LaneName + ", "
		+ FString::FromInt(TimeStep) + ").";
	//UE_LOG(LogTemp, Warning, TEXT("%s"), *(Atom));
	AddEvent(OtherActor->GetName(), Atom);
	Solve();
}


void AMonitor::OnExitMonitor(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	ActorToEventsMap.Remove(OtherActor->GetName());
	VehiclePointers.Remove(OtherActor->GetName());
}


std::string AMonitor::GetEventsString()
{
	FString EventsString;
	for (auto& Pair : ActorToEventsMap)
	{
		EventsString += Pair.Value;
	}
	return std::string(TCHAR_TO_ANSI(*EventsString));
}


void AMonitor::Solve()
{
	try {
		Clingo::Logger logger = [](Clingo::WarningCode, char const *message) {
			//UE_LOG(LogTemp, Warning, TEXT("Clingo logger message: %s"), ANSI_TO_TCHAR(message));
		};

		//std::string ProgramTitle = "#program time_" 
		//	+ std::to_string(FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 1000))
		//	+ ".\n";
		std::string EventsString = GetEventsString();
		//AppendToLogfile(ProgramTitle + EventsString);

		// Without the "-n 0" option, at most one model is found.
		Clingo::Control ctl{ {}, logger, 20 };

		ctl.add("base", {}, EventsString.c_str());
		ctl.add("base", {}, Geometry.c_str());
		ctl.add("base", {}, TrafficRules.c_str());

		ctl.ground({ {"base", {}} });
		auto solveHandle = ctl.solve();
		auto solveResult = solveHandle.get();
		if (solveResult.is_unsatisfiable())
		{
			UE_LOG(LogTemp, Error, TEXT("Not satisfiable!"));
		}
		if (solveResult.is_unknown())
		{
			UE_LOG(LogTemp, Error, TEXT("Satisfiability is unknown!"));
		}

		for (auto &model : solveHandle) {
			FString Model;
			for (auto &atom : model.symbols()) {
				if (atom.match("mustStopToYield", 1))
				{
					FString YieldingVehicleName = FString(atom.arguments()[0].name()).RightChop(2); // Chop "v_" off of the name
					ACarlaWheeledVehicle* YieldingVehicle = VehiclePointers[YieldingVehicleName];
					AWheeledVehicleAIController* Controller = Cast<AWheeledVehicleAIController>(YieldingVehicle->GetController());
					if (Controller != nullptr)
					{
						Controller->SetTrafficLightState(ETrafficLightState::Red);
						UE_LOG(LogTemp, Warning, TEXT("Setting %s's controller to stop!"), *YieldingVehicleName);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("%s's controller not found! (mustYield)"), *YieldingVehicleName);
					}
				}
				else if (atom.match("hasRightOfWay", 1))
				{
					FString VehicleName = FString(atom.arguments()[0].name()).RightChop(2);
					if (!VehiclePointers.Contains(VehicleName))
					{
						UE_LOG(LogTemp, Warning, TEXT("%s not found in VehiclePointers!"), *VehicleName);
					}
					else
					{
						ACarlaWheeledVehicle* Vehicle = VehiclePointers[VehicleName];
						AWheeledVehicleAIController* Controller = Cast<AWheeledVehicleAIController>(Vehicle->GetController());
						if (Controller != nullptr)
						{
							Controller->SetTrafficLightState(ETrafficLightState::Green);
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("%s's controller not found! (hasRightOfWay)"), *VehicleName);
						}

					}
				}
				FString AtomString(atom.to_string().c_str());
				Model.Append("\t" + AtomString + "\n");
			}
			UE_LOG(LogTemp, Error, TEXT("Clingo Model:\n%s\n"), *Model);

		}
	}
	catch (std::exception const &e) {
		UE_LOG(LogTemp, Warning, TEXT("Clingo failed with: %s"), ANSI_TO_TCHAR(e.what()));
	}
}

template <class ActorClass>
void AMonitor::GetIntersectingActors(TArray<ActorClass*>& OutArray)
{
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ActorClass::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		FVector MonitorToActor = Actor->GetActorLocation() - GetActorLocation();
		FVector Displacement = UKismetMathLibrary::InverseTransformDirection(GetActorTransform(), MonitorToActor);
		if ((abs(Displacement.X) <= ExtentBox->GetScaledBoxExtent().X)
			&& (abs(Displacement.Y) <= ExtentBox->GetScaledBoxExtent().Y))
		{
			ActorClass* ActorTyped = Cast<ActorClass>(Actor);
			OutArray.Add(ActorTyped);
		}
	}
}

FString AMonitor::SignalToString(EVehicleSignalState Signal)
{
	switch (Signal) {
	case EVehicleSignalState::Left:
		return FString("left");
	case EVehicleSignalState::Right:
		return FString("right");
	case EVehicleSignalState::Emergency:
		return FString("emergency");
	default:
		return FString("off");
	}
}