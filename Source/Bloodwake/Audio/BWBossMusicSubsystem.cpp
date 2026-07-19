// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/BWBossMusicSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

void UBWBossMusicSubsystem::PlayBossMusic(USoundBase* InBGM, AActor* Requester, float FadeInDuration)
{
	if (!InBGM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBWBossMusicSubsystem] PlayBossMusic: InBGM이 null입니다."));
		return;
	}

	// 같은 요청자가 이미 재생 중이면 무동작(Show/Hide 반복 호출 시 BGM이 처음부터 다시 시작되는 것 방지).
	if (CurrentRequester == Requester && IsValid(CurrentMusicComponent) && CurrentMusicComponent->IsPlaying())
	{
		return;
	}

	// 다른 보스(또는 같은 보스의 기존 컴포넌트)가 재생 중이면 먼저 페이드 아웃한다.
	if (IsValid(CurrentMusicComponent))
	{
		CurrentMusicComponent->FadeOut(0.3f, 0.f);
		CurrentMusicComponent = nullptr;
	}
	CurrentRequester.Reset();

	// 새 BGM 재생. bAutoDestroy=false 필수 — true이면 컴포넌트가 스스로 파괴되어
	// StopBossMusic 호출 시 정지할 대상이 사라진다(가장 흔한 실수).
	// ※ BGM 에셋 자체에 Looping이 켜져 있어야 전투 내내 반복 재생된다(코드로 강제하지 않음).
	CurrentMusicComponent = UGameplayStatics::SpawnSound2D(
		this,
		InBGM,
		/*VolumeMultiplier=*/1.f,
		/*PitchMultiplier=*/1.f,
		/*StartTime=*/0.f,
		/*ConcurrencySettings=*/nullptr,
		/*bPersistAcrossLevelTransition=*/false,
		/*bAutoDestroy=*/false
	);

	if (!IsValid(CurrentMusicComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UBWBossMusicSubsystem] PlayBossMusic: SpawnSound2D가 nullptr를 반환했습니다. 사운드 에셋을 확인하세요."));
		return;
	}

	CurrentMusicComponent->FadeIn(FadeInDuration);
	CurrentRequester = Requester;
}

void UBWBossMusicSubsystem::StopBossMusic(AActor* Requester, float FadeOutDuration)
{
	// Requester가 현재 소유자가 아니면 무동작(다른 보스의 BGM을 실수로 끄지 않음).
	if (CurrentRequester != Requester)
	{
		return;
	}

	if (IsValid(CurrentMusicComponent))
	{
		CurrentMusicComponent->FadeOut(FadeOutDuration, 0.f);
		CurrentMusicComponent = nullptr;
	}
	CurrentRequester.Reset();
}

void UBWBossMusicSubsystem::StopAllBossMusic(float FadeOutDuration)
{
	if (IsValid(CurrentMusicComponent))
	{
		CurrentMusicComponent->FadeOut(FadeOutDuration, 0.f);
		CurrentMusicComponent = nullptr;
	}
	CurrentRequester.Reset();
}

void UBWBossMusicSubsystem::Deinitialize()
{
	// PIE 종료 시에도 에디터에서 BGM이 계속 울리지 않도록 즉시 정지한다(CLAUDE.md 4.3).
	// Deinitialize를 빠뜨리면 PIE 종료 후에도 에디터에서 BGM이 계속 울리는 현상이 발생한다.
	if (IsValid(CurrentMusicComponent))
	{
		CurrentMusicComponent->Stop();
		CurrentMusicComponent = nullptr;
	}
	CurrentRequester.Reset();

	Super::Deinitialize();
}
