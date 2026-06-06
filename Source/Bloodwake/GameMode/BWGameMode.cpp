// Fill out your copyright notice in the Description page of Project Settings.


#include "BWGameMode.h"

#include "Character/BWPlayerCharacter.h"
#include "Player/BWPlayerController.h"

ABWGameMode::ABWGameMode()
{
	// 기본 폰/컨트롤러를 프로젝트 C++ 베이스 클래스로 지정한다.
	// 실제 게임에서는 이 클래스를 상속한 BP_GameMode가 BP 폰/컨트롤러로 덮어쓴다.
	DefaultPawnClass = ABWPlayerCharacter::StaticClass();
	PlayerControllerClass = ABWPlayerController::StaticClass();
}
