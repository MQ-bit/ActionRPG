// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ARLockOnMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UARLockOnMarkerWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transactional);
	}
	if (!WidgetTree->RootWidget)
	{
		UTextBlock* Triangle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Triangle"));
		Triangle->SetText(FText::FromString(TEXT("▼")));
		Triangle->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.15f, 0.1f)));
		Triangle->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Triangle->GetFont();
		Font.Size = 28;
		Triangle->SetFont(Font);
		WidgetTree->RootWidget = Triangle;
	}
	return Super::RebuildWidget();
}
