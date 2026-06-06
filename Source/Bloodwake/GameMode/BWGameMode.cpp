// Fill out your copyright notice in the Description page of Project Settings.

#include "GameMode/BWGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Character/BWPlayerCharacter.h"
#include "Combat/BWAttributeComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/BWPlayerController.h"
#include "UI/BWMainHUDWidget.h"

ABWGameMode::ABWGameMode()
{
	// 기본 폰/컨트롤러를 프로젝트 C++ 베이스 클래스로 지정한다.
	// 실제 게임에서는 이 클래스를 상속한 BP_GameMode가 BP 폰/컨트롤러로 덮어쓴다.
	DefaultPawnClass = ABWPlayerCharacter::StaticClass();
	PlayerControllerClass = ABWPlayerController::StaticClass();
}

void ABWGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!MainHUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::BeginPlay — MainHUDWidgetClass가 설정되지 않았습니다. BP_GameMode에서 WBP_MainHUD를 지정하세요."));
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::BeginPlay — PlayerController를 찾을 수 없습니다."));
		return;
	}

	// 위젯 생성·AddToViewport는 이 틱에 처리한다.
	MainHUDWidget = CreateWidget<UBWMainHUDWidget>(PC, MainHUDWidgetClass);
	if (!IsValid(MainHUDWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::BeginPlay — MainHUDWidget 생성에 실패했습니다."));
		return;
	}

	MainHUDWidget->AddToViewport();

	// InitializeForPlayer(Attribute 읽기)는 한 틱 뒤로 미룬다.
	// GameMode::BeginPlay → Character::BeginPlay → Component::BeginPlay 순으로 실행되므로,
	// 이 시점엔 AttributeComponent의 CurrentHealth/Stamina/Focus가 아직 0(헤더 기본값)이다.
	// 다음 틱에 실행하면 모든 BeginPlay가 완료되어 MaxValue로 초기화된 값을 읽을 수 있다.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ABWGameMode::InitializeHUD);
}

void ABWGameMode::InitializeHUD()
{
	// 위젯이 아직 유효한지 확인 (극히 드물지만 위젯이 1틱 사이에 소멸할 수 있음)
	if (!IsValid(MainHUDWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::InitializeHUD — MainHUDWidget이 유효하지 않습니다."));
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::InitializeHUD — PlayerController를 찾을 수 없습니다."));
		return;
	}

	ABWPlayerCharacter* PlayerCharacter = Cast<ABWPlayerCharacter>(PC->GetPawn());
	if (!IsValid(PlayerCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::InitializeHUD — ABWPlayerCharacter를 찾을 수 없습니다."));
		return;
	}

	// FindComponentByClass 우회 없이 공개 접근자로 직접 접근한다.
	UBWAttributeComponent* Attributes = PlayerCharacter->GetAttributeComponent();
	if (!IsValid(Attributes))
	{
		UE_LOG(LogTemp, Warning, TEXT("ABWGameMode::InitializeHUD — AttributeComponent가 유효하지 않습니다."));
		return;
	}

	MainHUDWidget->InitializeForPlayer(Attributes);
}
