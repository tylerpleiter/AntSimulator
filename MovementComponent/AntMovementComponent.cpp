// Fill out your copyright notice in the Description page of Project Settings.


#include "AntMovementComponent.h"
#include "Engine/World.h"
#include "Math/UnrealMathVectorCommon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/Quat.h"
#include "AntBase.h"



UAntMovementComponent::UAntMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) {

	// Setup initial variable values

	AntOwner = Cast<AAntBase>(PawnOwner);

	MaxSpeed = 1000;
	Acceleration = FVector::ZeroVector;
	FrictionCoefficient = 1000.f;
	GripStrength = 100;
	isFalling = 0;
	EnableGravity = true;
	DeltaRot = { 2, 2, 2 };

}


void UAntMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Get accumulated user input
	FVector InputVector = FVector::ZeroVector;
	InputVector = ConsumeInputVector();

	//const AController* Controller = PawnOwner->GetController();

	// Get ground rotation and calculate current rotation
	FRotator RotationTarget = TraceGround(40.f, 30.f, 30.f);

	// If character is falling, remove any pitch or roll
	if (isFalling) {
		RotationTarget.Pitch = 0.f;
		RotationTarget.Roll = 0.f;
	}

	// Interpolate to target rotation
	FRotator Rotation = UKismetMathLibrary::RLerp(UpdatedComponent->GetComponentRotation(), RotationTarget, 0.15, true);

	// Calculate acceleration to apply
	Acceleration = 10000 * (InputVector).GetClampedToMaxSize(1.0f);

	// Add surface friction
	FVector Friction = Velocity * FrictionCoefficient;
	Friction -= (UpdatedComponent->GetUpVector() | Friction) * UpdatedComponent->GetUpVector();
	Acceleration -= Friction * DeltaTime;
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Friction: %f"), Friction.Size()));

	// Calculate and apply pending impulse and force
	// Ants have 'sticky' feet, so if the ant is on a surface, external forces only take effect if they overcome this 'stickiness/grip'
	if ((PendingImpulse + PendingForce).Size() > GripStrength) {
		isFalling = true;
		Velocity += PendingImpulse + PendingForce * DeltaTime;
	}
	PendingForce = FVector::ZeroVector;
	PendingImpulse = FVector::ZeroVector;

	// Apply gravity force if ant is falling
	if (isFalling && EnableGravity) {
		Velocity += FVector{ 0, 0, GetGravityZ() * 1 } * DeltaTime;
	}

	// Apply acceleration to velocity
	Velocity += Acceleration * DeltaTime;
	Velocity = Velocity.GetClampedToMaxSize(3000);

	// Get change in location using velocity and time
	FVector DeltaLocation = Velocity * DeltaTime;

	// Move component
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(DeltaLocation, Rotation.Quaternion(), true, Hit);
	//UpdatedComponent->AddLocalRotation(ControllerRotation.Quaternion());

	// Only set not falling if velocity low and ground hit
	if (Hit.bBlockingHit && Velocity.Size() < 1000)
		isFalling = false;


	if (Hit.bBlockingHit) {
		// Kill component of velocity in hit normal
		Velocity -= (Hit.Normal | Velocity) * Hit.Normal;
		SlideAlongSurface(DeltaLocation, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}
	else {
		if (!isFalling) {
			// If no hit and not falling, sweep downwards and teleport player to ground if within reach
			FCollisionShape ColSphere = FCollisionShape::MakeSphere(10.f);
			TArray<FHitResult> Hits;
			FVector ActorLoc = UpdatedComponent->GetComponentLocation();
			if (GetWorld()->SweepMultiByChannel(Hits, ActorLoc, ActorLoc - UpdatedComponent->GetUpVector() * 30, FQuat::Identity, ECC_Visibility, ColSphere)) {
				SafeMoveUpdatedComponent(Hits.Last().Location - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
				Velocity -= (Hit.Normal | Velocity) * Hit.Normal;
				//DrawDebugSphere(GetWorld(), Hits.Last().Location, ColSphere.GetSphereRadius(), 30.f, FColor::Red, false, -1.f, 0, 1.0f);
			}
			else {
				isFalling = true;
			}
		}
	}
	
	//DrawDebugDirectionalArrow(GetWorld(), UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentLocation() + Velocity / 10, 50, FColor::Red, false, -1.f, 0, 5.f);
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Velocity: %f"), Velocity.Size()));

}


FRotator UAntMovementComponent::TraceGround(float ForwardDistance, float RightDistance, float DownDistance) {

	// Line trace 4 points from center to outer legs
	FHitResult FLHit, FRHit, BLHit, BRHit;

	FVector TraceStart = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * 10.f;
	FVector FLTraceEnd = TraceStart + UpdatedComponent->GetForwardVector() * ForwardDistance + UpdatedComponent->GetRightVector() * -RightDistance + UpdatedComponent->GetUpVector() * -DownDistance;
	FVector FRTraceEnd = TraceStart + UpdatedComponent->GetForwardVector() * ForwardDistance + UpdatedComponent->GetRightVector() * RightDistance + UpdatedComponent->GetUpVector() * -DownDistance;
	FVector BLTraceEnd = TraceStart + UpdatedComponent->GetForwardVector() * -ForwardDistance + UpdatedComponent->GetRightVector() * -RightDistance + UpdatedComponent->GetUpVector() * -DownDistance;
	FVector BRTraceEnd = TraceStart + UpdatedComponent->GetForwardVector() * -ForwardDistance + UpdatedComponent->GetRightVector() * RightDistance + UpdatedComponent->GetUpVector() * -DownDistance;
	FVector CTraceEnd = TraceStart + UpdatedComponent->GetUpVector() * -DownDistance * 2;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	GetWorld()->LineTraceSingleByChannel(FLHit, TraceStart, FLTraceEnd, ECC_Visibility, QueryParams);
	//DrawDebugLine(GetWorld(), TraceStart, FLTraceEnd, FLHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	if (!FLHit.bBlockingHit) {
		GetWorld()->LineTraceSingleByChannel(FLHit, FLTraceEnd, CTraceEnd, ECC_Visibility, QueryParams);
		//DrawDebugLine(GetWorld(), FLTraceEnd, CTraceEnd, FLHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	}

	GetWorld()->LineTraceSingleByChannel(FRHit, TraceStart, FRTraceEnd, ECC_Visibility, QueryParams);
	//DrawDebugLine(GetWorld(), TraceStart, FRTraceEnd, FRHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	if (!FRHit.bBlockingHit) {
		GetWorld()->LineTraceSingleByChannel(FRHit, FRTraceEnd, CTraceEnd, ECC_Visibility, QueryParams);
		//DrawDebugLine(GetWorld(), FRTraceEnd, CTraceEnd, FRHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	}

	GetWorld()->LineTraceSingleByChannel(BLHit, TraceStart, BLTraceEnd, ECC_Visibility, QueryParams);
	//DrawDebugLine(GetWorld(), TraceStart, BLTraceEnd, BLHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	if (!BLHit.bBlockingHit) {
		GetWorld()->LineTraceSingleByChannel(BLHit, BLTraceEnd, CTraceEnd, ECC_Visibility, QueryParams);
		//DrawDebugLine(GetWorld(), BLTraceEnd, CTraceEnd, BLHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	}

	GetWorld()->LineTraceSingleByChannel(BRHit, TraceStart, BRTraceEnd, ECC_Visibility, QueryParams);
	//DrawDebugLine(GetWorld(), TraceStart, BRTraceEnd, BRHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	if (!BRHit.bBlockingHit) {
		GetWorld()->LineTraceSingleByChannel(BRHit, BRTraceEnd, CTraceEnd, ECC_Visibility, QueryParams);
		//DrawDebugLine(GetWorld(), BRTraceEnd, CTraceEnd, BRHit.bBlockingHit ? FColor::Red : FColor::Green, false, -1.f, 0, 1.0f);
	}

#if 0
	// Check if ground has been hit at all, otherwise we're probabaly falling
	if (FLHit.bBlockingHit || FRHit.bBlockingHit || BLHit.bBlockingHit || BRHit.bBlockingHit)
		isFalling = false;
	else
		isFalling = true;
#endif

	// Get locations used for rotation calculations
	FVector FLPoint, FRPoint, BLPoint, BRPoint, FPoint, RPoint, LPoint, BPoint;
	FLPoint = FLHit.bBlockingHit ? FLHit.Location : (FLTraceEnd - UpdatedComponent->GetUpVector() * DownDistance);
	FRPoint = FRHit.bBlockingHit ? FRHit.Location : (FRTraceEnd - UpdatedComponent->GetUpVector() * DownDistance);
	BLPoint = BLHit.bBlockingHit ? BLHit.Location : (BLTraceEnd - UpdatedComponent->GetUpVector() * DownDistance);
	BRPoint = BRHit.bBlockingHit ? BRHit.Location : (BRTraceEnd - UpdatedComponent->GetUpVector() * DownDistance);

	// Calculate mid points
	FPoint = FMath::Lerp(FLPoint, FRPoint, 0.5);
	RPoint = FMath::Lerp(FRPoint, BRPoint, 0.5);
	LPoint = FMath::Lerp(FLPoint, BLPoint, 0.5);
	BPoint = FMath::Lerp(BLPoint, BRPoint, 0.5);

	// Calulate direction vectors
	FVector Forward = UKismetMathLibrary::GetDirectionUnitVector(BPoint, FPoint);
	FVector Right = UKismetMathLibrary::GetDirectionUnitVector(LPoint, RPoint);

	// Calculate rotator to ground normal
	FRotator RotationGround = UKismetMathLibrary::MakeRotFromXY(Forward, Right);

	return RotationGround;
}

void UAntMovementComponent::AddImpulse(FVector Impulse)
{
	PendingImpulse += Impulse;
}

void UAntMovementComponent::AddForce(FVector Force)
{
	PendingForce += Force;
}
