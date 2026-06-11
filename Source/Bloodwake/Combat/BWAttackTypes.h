// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "BWAttackTypes.generated.h"

/**
 * 공격 종류 열거형.
 * DataTable RowName 키와 1:1 대응한다 (GetRowNameForAttackType 참조).
 * Light/Heavy는 콤보형(다단계), Running/Special은 단발형(Steps 1개).
 */
UENUM(BlueprintType)
enum class EBWAttackType : uint8
{
	Light   UMETA(DisplayName = "Light"),
	Running UMETA(DisplayName = "Running"),
	Special UMETA(DisplayName = "Special"),
	Heavy   UMETA(DisplayName = "Heavy"),
};

/**
 * 입력 진입점에서 받는 입력 종류.
 * IA_Attack(PrimaryTap/PrimaryHold)과 IA_HeavyAttack(Heavy)을 구분한다.
 */
UENUM(BlueprintType)
enum class EBWAttackInputKind : uint8
{
	PrimaryTap  UMETA(DisplayName = "Primary Tap"),
	PrimaryHold UMETA(DisplayName = "Primary Hold"),
	Heavy       UMETA(DisplayName = "Heavy"),
};

/**
 * 콤보 한 단계를 정의하는 구조체.
 * DataTable FBWAttackComboRow.Steps 배열의 원소.
 * 몽타주와 스태미나 소모값을 한 쌍으로 보유한다.
 */
USTRUCT(BlueprintType)
struct BLOODWAKE_API FBWComboStep
{
	GENERATED_BODY()

	/** 이 콤보 단계에서 재생할 몽타주. 디자이너가 DataTable에서 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 이 단계 발동에 소비할 스태미나. 부족하면 발동 불가. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (ClampMin = "0.0"))
	float StaminaCost = 10.f;
};

/**
 * DataTable Row 구조체. RowName = 공격 종류(Light/Running/Special/Heavy).
 * 콤보 단계 목록을 TArray로 보유한다.
 * 단발 공격(Running/Special)은 Steps 원소 1개, 콤보(Light/Heavy)는 4개 이상.
 */
USTRUCT(BlueprintType)
struct BLOODWAKE_API FBWAttackComboRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 콤보 단계 목록. Steps.Num() = 콤보 길이. 순환은 인덱스 % Num()으로 처리한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TArray<FBWComboStep> Steps;
};
