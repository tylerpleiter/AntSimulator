// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"

#include "AntMovementComponent.generated.h"

class AAntBase;
class UAntMovementComponent;

/**
 * 
 */
UCLASS()
class ANTMOVEMENT_API UAntMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:

	/**
	 * Default UObject constructor.
	 */
	UAntMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	AAntBase* AntOwner;

	// Pending impulse to apply
	FVector PendingImpulse;

	// Pending force to apply
	FVector PendingForce;

	// Current actor upwards direction
	FVector UpVector;

	// Falling flag
	uint8_t isFalling;

public:


	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void AddImpulse(FVector Impulse);
	void AddForce(FVector Force);




	UPROPERTY(EditAnywhere)
	float MaxSpeed;

	UPROPERTY(EditAnywhere)
	FVector Acceleration;

	UPROPERTY(EditAnywhere)
	float GripStrength;

	UPROPERTY(EditAnywhere)
	bool EnableGravity;

	UPROPERTY(EditAnywhere)
	float FrictionCoefficient;

	UPROPERTY(EditAnywhere)
	FRotator DeltaRot;


private:

	// Returns a rotation based on current ground
	FRotator TraceGround(float Forward, float Right, float Down);

};
