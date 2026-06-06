// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BWPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UInputComponent;
struct FInputActionValue;

/**
 * 플레이어가 조작하는 3인칭 소울라이크 전투 캐릭터의 베이스 클래스.
 * 락온 기반 근접 전투를 위한 카메라 붐 + 팔로우 카메라 구성을 제공한다.
 * 스태미나/포이즈/콤보 등 전투 시스템은 후속 작업에서 컴포넌트로 분리해 부착한다.
 */
UCLASS()
class BLOODWAKE_API ABWPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABWPlayerCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	/** MoveAction 콜백: 컨트롤러 Yaw 기준으로 입력 벡터를 월드 이동 방향으로 변환해 이동한다. */
	void Move(const FInputActionValue& Value);

	/** LookAction 콜백: 마우스/우스틱 입력을 컨트롤러 회전(Yaw/Pitch)에 적용한다. */
	void Look(const FInputActionValue& Value);

protected:
	/** 캐릭터와 카메라 사이 거리를 두고 충돌을 보정하는 카메라 붐. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 카메라 붐 끝에 부착되는 3인칭 팔로우 카메라. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UInputAction> LookAction;
};
