// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BWBossMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 보스 BGM의 배타적 재생·정지·페이드 아웃을 담당하는 월드 서브시스템.
 * 동시에 하나의 BGM만 유지하며, 보스 사망·레벨 전환 시 확실히 정지한다.
 * 접근: GetWorld()->GetSubsystem<UBWBossMusicSubsystem>()
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWBossMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 보스 BGM 재생 요청. Requester(보스)를 현재 소유자로 등록한다.
	 * 같은 Requester가 이미 재생 중이면 무동작(중복 재생 방지 — Show/Hide 반복 호출 안전).
	 * 다른 보스가 재생 중이면 기존 BGM을 즉시 페이드 아웃하고 교체한다.
	 * InBGM이 null이면 Warning 로그 후 무동작.
	 * ※ BGM 에셋 자체에 Looping이 켜져 있어야 전투 내내 반복 재생된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|BossMusic")
	void PlayBossMusic(USoundBase* InBGM, AActor* Requester, float FadeInDuration = 1.0f);

	/**
	 * 보스 BGM 정지 요청. Requester가 현재 소유자일 때만 정지한다(다른 보스의 BGM을 실수로 끄지 않음).
	 * 재생 중이 아니면 무동작(안전 중복 호출 허용).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio|BossMusic")
	void StopBossMusic(AActor* Requester, float FadeOutDuration = 1.5f);

	/** 소유자와 무관하게 즉시 정지한다. 레벨 전환·강제 종료용. */
	UFUNCTION(BlueprintCallable, Category = "Audio|BossMusic")
	void StopAllBossMusic(float FadeOutDuration = 0.5f);

protected:
	/**
	 * 재생 중인 BGM을 확실히 정지한다(누수·댕글링 방지 — CLAUDE.md 4.3).
	 * PIE 종료 시에도 에디터에서 BGM이 계속 울리지 않도록 반드시 구현해야 한다.
	 */
	virtual void Deinitialize() override;

private:
	/**
	 * 현재 재생 중인 2D BGM 오디오 컴포넌트.
	 * UGameplayStatics::SpawnSound2D(bAutoDestroy=false)로 생성해 수동 수명 관리한다.
	 * GC 추적 필수 — TObjectPtr + UPROPERTY(Transient) (CLAUDE.md 3.3).
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CurrentMusicComponent;

	/**
	 * 현재 BGM을 요청한 보스. 소유권 판정용 약참조(보스가 파괴돼도 댕글링 없음).
	 * 소유하지 않는 참조이므로 TWeakObjectPtr (CLAUDE.md 3.3).
	 */
	TWeakObjectPtr<AActor> CurrentRequester;
};
