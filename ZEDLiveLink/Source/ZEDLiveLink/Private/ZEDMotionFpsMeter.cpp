#include "ZEDMotionFpsMeter.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogZEDMotionFps, Log, All);

namespace ZEDMotionFps
{
	// On-screen message keys, so each line is replaced in place instead of stacking up.
	constexpr uint64 StatusKey = 0x5A45444D46505300ull;
	constexpr uint64 AverageKey = StatusKey + 100;
	constexpr float ResultDisplaySeconds = 3600.f;

	uint64 RunKey(int32 RunIndex)
	{
		return StatusKey + 1 + RunIndex;
	}

	void ShowOnScreen(uint64 Key, float DisplaySeconds, const FColor& Color, const FString& Message)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(Key, DisplaySeconds, Color, Message, true, FVector2D(1.5f, 1.5f));
		}
	}

	FAutoConsoleCommand StartCommand(
		TEXT("ZED.MotionFps.Start"),
		TEXT("Counts the ZED motion frames received for 30 s and prints the FPS. Run it 5 times to get the average."),
		FConsoleCommandDelegate::CreateLambda([]() { FZEDMotionFpsMeter::Get().StartRun(); }));

	FAutoConsoleCommand ResetCommand(
		TEXT("ZED.MotionFps.Reset"),
		TEXT("Clears the ZED motion FPS runs."),
		FConsoleCommandDelegate::CreateLambda([]() { FZEDMotionFpsMeter::Get().Reset(); }));
}

FZEDMotionFpsMeter& FZEDMotionFpsMeter::Get()
{
	static FZEDMotionFpsMeter Instance;
	return Instance;
}

void FZEDMotionFpsMeter::OnMotionFrame(int32 FrameID)
{
	if (!bRunning)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - RunStartSeconds >= RunDurationSeconds)
	{
		// The window is over: close the run before counting anything received after it.
		FinishRun(Now);
		return;
	}

	// The sender may send several bodies of one camera frame: count each frame_id once.
	if (FrameID != LastFrameID)
	{
		LastFrameID = FrameID;
		++RunFrames;
	}
}

void FZEDMotionFpsMeter::StartRun()
{
	if (bRunning)
	{
		UE_LOG(LogZEDMotionFps, Warning, TEXT("Run %d/%d is still in progress."), Runs.Num() + 1, NumRuns);
		return;
	}

	if (Runs.Num() >= NumRuns)
	{
		Reset();
	}

	if (Runs.Num() == 0)
	{
		CsvPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ZEDMotionFps"),
			FString::Printf(TEXT("MotionFps_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
		AppendCsvLine(TEXT("run,frames,seconds,fps"));
	}

	StopTicker();
	bRunning = true;
	RunFrames = 0;
	LastFrameID = INDEX_NONE;
	RunStartSeconds = FPlatformTime::Seconds();
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FZEDMotionFpsMeter::Tick));

	UE_LOG(LogZEDMotionFps, Display, TEXT("Run %d/%d started: counting motion frames for %.0f s."), Runs.Num() + 1, NumRuns, RunDurationSeconds);
}

void FZEDMotionFpsMeter::Reset()
{
	StopTicker();
	bRunning = false;
	Runs.Reset();

	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(ZEDMotionFps::StatusKey);
		GEngine->RemoveOnScreenDebugMessage(ZEDMotionFps::AverageKey);
		for (int32 RunIndex = 1; RunIndex <= NumRuns; ++RunIndex)
		{
			GEngine->RemoveOnScreenDebugMessage(ZEDMotionFps::RunKey(RunIndex));
		}
	}
	UE_LOG(LogZEDMotionFps, Display, TEXT("Runs cleared."));
}

void FZEDMotionFpsMeter::Shutdown()
{
	StopTicker();
	bRunning = false;
}

double FZEDMotionFpsMeter::GetAverageFps() const
{
	if (Runs.Num() == 0)
	{
		return 0.0;
	}

	double Sum = 0.0;
	for (const FRunResult& Run : Runs)
	{
		Sum += Run.Fps;
	}
	return Sum / Runs.Num();
}

bool FZEDMotionFpsMeter::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		// The run was closed by OnMotionFrame: unregister.
		TickerHandle.Reset();
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - RunStartSeconds;
	if (Elapsed >= RunDurationSeconds)
	{
		FinishRun(Now);
		TickerHandle.Reset();
		return false;
	}

	ZEDMotionFps::ShowOnScreen(ZEDMotionFps::StatusKey, 1.f, FColor::Cyan,
		FString::Printf(TEXT("ZED motion FPS - run %d/%d: %.1f / %.0f s, %d frames (%.1f fps)"),
			Runs.Num() + 1, NumRuns, Elapsed, RunDurationSeconds, RunFrames, Elapsed > 0.0 ? RunFrames / Elapsed : 0.0));
	return true;
}

void FZEDMotionFpsMeter::FinishRun(double Now)
{
	bRunning = false;

	// FPS = frames counted / time actually measured (start to the first check after 30 s).
	const double Seconds = Now - RunStartSeconds;
	const double Fps = Seconds > 0.0 ? RunFrames / Seconds : 0.0;
	Runs.Add({ RunFrames, Seconds, Fps });
	const int32 RunIndex = Runs.Num();

	const FString Result = FString::Printf(TEXT("Run %d/%d: %d frames / %.3f s = %.2f fps"), RunIndex, NumRuns, RunFrames, Seconds, Fps);
	UE_LOG(LogZEDMotionFps, Display, TEXT("%s"), *Result);
	AppendCsvLine(FString::Printf(TEXT("%d,%d,%.3f,%.2f"), RunIndex, RunFrames, Seconds, Fps));
	ZEDMotionFps::ShowOnScreen(ZEDMotionFps::RunKey(RunIndex), ZEDMotionFps::ResultDisplaySeconds, FColor::Green, Result);

	if (RunIndex < NumRuns)
	{
		ZEDMotionFps::ShowOnScreen(ZEDMotionFps::StatusKey, ZEDMotionFps::ResultDisplaySeconds, FColor::Cyan,
			FString::Printf(TEXT("ZED motion FPS - run %d/%d done. Run ZED.MotionFps.Start for run %d."), RunIndex, NumRuns, RunIndex + 1));
		return;
	}

	const double Average = GetAverageFps();
	const FString Summary = FString::Printf(TEXT("Average of %d runs: %.2f fps (criterion >= %.0f fps: %s)"),
		NumRuns, Average, MinimumFps, Average >= MinimumFps ? TEXT("PASS") : TEXT("FAIL"));
	UE_LOG(LogZEDMotionFps, Display, TEXT("%s"), *Summary);
	AppendCsvLine(FString::Printf(TEXT("average,,,%.2f"), Average));
	ZEDMotionFps::ShowOnScreen(ZEDMotionFps::AverageKey, ZEDMotionFps::ResultDisplaySeconds, FColor::Yellow, Summary);
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(ZEDMotionFps::StatusKey);
	}
}

void FZEDMotionFpsMeter::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void FZEDMotionFpsMeter::AppendCsvLine(const FString& Line) const
{
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

void UZEDMotionFpsLibrary::StartMotionFpsRun()
{
	FZEDMotionFpsMeter::Get().StartRun();
}

void UZEDMotionFpsLibrary::ResetMotionFps()
{
	FZEDMotionFpsMeter::Get().Reset();
}

bool UZEDMotionFpsLibrary::IsMotionFpsRunning()
{
	return FZEDMotionFpsMeter::Get().IsRunning();
}

int32 UZEDMotionFpsLibrary::GetMotionFpsCompletedRuns()
{
	return FZEDMotionFpsMeter::Get().GetNumCompletedRuns();
}

float UZEDMotionFpsLibrary::GetMotionFpsAverage()
{
	return static_cast<float>(FZEDMotionFpsMeter::Get().GetAverageFps());
}
