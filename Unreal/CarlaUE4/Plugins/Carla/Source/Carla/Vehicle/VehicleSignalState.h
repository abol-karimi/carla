// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "VehicleSignalState.generated.h"

UENUM(BlueprintType)
enum class EVehicleSignalState : uint8
{
	Off			UMETA(DisplayName = "Off"),
	Right		UMETA(DisplayName = "Right"),
	Left		UMETA(DisplayName = "Left"),
	Emergency	UMETA(DisplayName = "Emergency")
};