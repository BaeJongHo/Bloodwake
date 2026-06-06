// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BWPlayerController.generated.h"

/**
 * 플레이어 입력을 담당하는 컨트롤러 베이스 클래스.
 * 키보드&마우스 + 게임패드를 동등하게 다루는 Enhanced Input 매핑 컨텍스트를
 * 후속 작업에서 이곳에서 등록한다(키 하드코딩 금지 — IMC_/IA_ 참조만 사용).
 */
UCLASS()
class BLOODWAKE_API ABWPlayerController : public APlayerController
{
	GENERATED_BODY()
};
