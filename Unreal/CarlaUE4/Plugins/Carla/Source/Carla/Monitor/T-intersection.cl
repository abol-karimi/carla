%-----------------Traffic--------------
arrived(Vehicle):-
  arrivesAtForkAtTime(Vehicle, _, _).

exited(Vehicle):-
  leavesExitAtTime(Vehicle, _, _).

inTheIntersection(Vehicle):-
  arrived(Vehicle),
  not exited(Vehicle).

entered(Vehicle):-
  entersForkAtTime(Vehicle, _, _).

atTheIntersection(Vehicle):-
  arrived(Vehicle),
  not entered(Vehicle).

isAtFork(Vehicle, Fork):-
  arrivesAtForkAtTime(Vehicle, Fork, Time),
  atTheIntersection(Vehicle).

arrivedEarlierThan(Vehicle1, Vehicle2):-
  arrivesAtForkAtTime(Vehicle1, _, ArrivalTime1),
  arrivesAtForkAtTime(Vehicle2, _, ArrivalTime2),
  ArrivalTime1 < ArrivalTime2.

arrivedSameTime(Vehicle1, Vehicle2):-
  arrivesAtForkAtTime(Vehicle1, _, ArrivalTime),
  arrivesAtForkAtTime(Vehicle2, _, ArrivalTime).

isOnRightOf(Vehicle1, Vehicle2):-
  isAtFork(Vehicle1, Fork1),
  isAtFork(Vehicle2, Fork2),
  isOnRightOf(Fork1, Fork2).

leftTheLane(Vehicle, Lane):-
  leavesLaneAtTime(Vehicle, Lane, _).

% Does not handle re-entries.
isOnLane(Vehicle, Lane):-
  entersLaneAtTime(Vehicle, Lane, _),
  not leftTheLane(Vehicle, Lane).

branchOf(Lane, Fork):-
  laneFromTo(Lane, Fork, _).

signaledAtFork(Vehicle, Signal, Fork):-
  signalsAtForkAtTime(Vehicle, Signal, Fork, _).

forkOnThroughRoad(Fork):-
  laneFromTo(Lane, Fork, _),
  laneCorrectSignal(Lane, off).

vehicleOnThroughRoad(Vehicle):-
  arrivesAtForkAtTime(Vehicle, Fork, _),
  forkOnThroughRoad(Fork),
  not exited(Vehicle).
%------------------------------------------
%--------------Rules pre-definitions-------
%------------------------------------------
requestedLane(Vehicle, Lane):-
  signaledAtFork(Vehicle, Signal, Fork),
  branchOf(Lane, Fork),
  laneCorrectSignal(Lane, Signal).

% vehicle on the planned lane
reservedLane(Vehicle, Lane1):-
  isOnLane(Vehicle, Lane1),
  requestedLane(Vehicle, Lane1).

reservedLane(Vehicle, Lane1):-
  isOnLane(Vehicle, Lane2),
  requestedLane(Vehicle, Lane2),
  overlaps(Lane2, Lane1),
  not leftTheLane(Vehicle, Lane1).

%---------------- Rules ---------------
% Page 35:
% At intersections without `STOP' or `YIELD' signs,
%  yield to traffic and pedestrians already in the intersection
%  or just entering the intersection.
mustYieldToForRule(Vehicle1, Vehicle2, yieldToInside):-
  atTheIntersection(Vehicle1),
  inTheIntersection(Vehicle2).

mustSlowToYield(Vehicle1):-
  mustYieldToForRule(Vehicle1, Vehicle2, yieldToInside),
  vehicleOnThroughRoad(Vehicle1),
  requestedLane(Vehicle1, Lane),
  reservedLane(Vehicle2, Lane).

% Page 35:
% At “T” intersections without “STOP” or “YIELD” signs,
% yield to traffic and pedestrians on the through road.
% They have the right-of-way.

mustYieldToForRule(Vehicle1, Vehicle2, throughRoadFirst):-
  atTheIntersection(Vehicle1),
  vehicleOnThroughRoad(Vehicle2),
  not vehicleOnThroughRoad(Vehicle1).

mustStopToYield(Vehicle1):-
  mustYieldToForRule(Vehicle1, Vehicle2, throughRoadFirst),
  requestedLane(Vehicle1, Lane1),
  requestedLane(Vehicle2, Lane2),
  overlaps(Lane1, Lane2).

%-------------------------------------------------
needNotStop(Vehicle):-
  arrived(Vehicle),
  not mustStopToYield(Vehicle).

#show mustYieldToForRule/3.
#show mustStopToYield/1.
#show mustSlowToYield/1.
#show needNotStop/1.
