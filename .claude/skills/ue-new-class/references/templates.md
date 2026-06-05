# UE 5.7 클래스 골격 템플릿

`<ClassName>`(접두사 제외 파일명 기준), `<MODULE_API>`(예: `MYGAME_API`)를 프로젝트 값으로 치환한다.
`<MODULE_API>` 매크로는 같은 모듈의 다른 클래스를 참고하면 정확한 이름을 알 수 있다.

---

## AActor

```cpp
// <ClassName>.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "<ClassName>.generated.h"

UCLASS()
class <MODULE_API> A<ClassName> : public AActor
{
    GENERATED_BODY()

public:
    A<ClassName>();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaSeconds) override;
};
```

```cpp
// <ClassName>.cpp
#include "<ClassName>.h"

A<ClassName>::A<ClassName>()
{
    PrimaryActorTick.bCanEverTick = false; // 필요할 때만 true
}

void A<ClassName>::BeginPlay()
{
    Super::BeginPlay();
}

void A<ClassName>::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
}
```

---

## UActorComponent

```cpp
// <ClassName>.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "<ClassName>.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class <MODULE_API> U<ClassName> : public UActorComponent
{
    GENERATED_BODY()

public:
    U<ClassName>();

protected:
    virtual void BeginPlay() override;
};
```

```cpp
// <ClassName>.cpp
#include "<ClassName>.h"

U<ClassName>::U<ClassName>()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void U<ClassName>::BeginPlay()
{
    Super::BeginPlay();
}
```

---

## UObject (데이터/로직)

```cpp
// <ClassName>.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "<ClassName>.generated.h"

UCLASS(BlueprintType)
class <MODULE_API> U<ClassName> : public UObject
{
    GENERATED_BODY()
};
```

---

## UGameInstanceSubsystem (전역 시스템)

```cpp
// <ClassName>.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "<ClassName>.generated.h"

UCLASS()
class <MODULE_API> U<ClassName> : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
};
```

```cpp
// <ClassName>.cpp
#include "<ClassName>.h"

void U<ClassName>::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void U<ClassName>::Deinitialize()
{
    Super::Deinitialize();
}
```

> 접근: `GetGameInstance()->GetSubsystem<U<ClassName>>()`

---

## 소유 포인터 멤버 예 (GC 추적)

```cpp
// 헤더 — 다른 UObject를 소유/참조하는 멤버
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
TObjectPtr<UStaticMeshComponent> MeshComponent;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Config")
TObjectPtr<UDataAsset> ConfigData;

// 소유하지 않는 약한 참조
UPROPERTY()
TWeakObjectPtr<AActor> TargetActor;
```

```cpp
// 생성자 — 컴포넌트 생성
MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
RootComponent = MeshComponent;
```
