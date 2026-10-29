#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZEDMotionFpsMeter.generated.h"

/**
 * Measures the motion tracking update rate on the Unreal side.
 *
 * A run counts the Animation frames FZEDLiveLinkSource pushes to LiveLink for RunDurationSeconds,
 * timed with FPlatformTime so the run stops by itself, and divides the count by the measured time.
 * After NumRuns runs the average is computed. Every result is shown on screen, written to the log
 * and appended to Saved/ZEDMotionFps/MotionFps_<date>.csv.
 *
 * Start a run with the console command ZED.MotionFps.Start (ZED.MotionFps.Reset clears the runs)
 * or from Blueprint with UZEDMotionFpsLibrary.
 */
class ZEDLIVELINK_API FZEDMotionFpsMeter
{
public:
	static constexpr double RunDurationSeconds = 30.0;
	static constexpr int32 NumRuns = 5;
	static constexpr double MinimumFps = 12.0;

	static FZEDMotionFpsMeter& Get();

	/** Counts one motion frame. Called on the game thread for every Animation frame pushed to LiveLink. */
	void OnMotionFrame(int32 FrameID);

	/** Starts the next run; after NumRuns runs a new series starts. Ignored while a run is active. */
	void StartRun();

	/** Stops any active run and clears the results. */
	void Reset();

	/** Stops the ticker. Called when the module shuts down. */
	void Shutdown();

	bool IsRunning() const { return bRunning; }
	int32 GetNumCompletedRuns() const { return Runs.Num(); }
	double GetAverageFps() const;

private:
	struct FRunResult
	{
		int32 Frames;
		double Seconds;
		double Fps;
	};

	bool Tick(float DeltaTime);
	void FinishRun(double Now);
	void StopTicker();
	void AppendCsvLine(const FString& Line) const;

	bool bRunning = false;
	double RunStartSeconds = 0.0;
	int32 RunFrames = 0;
	int32 LastFrameID = INDEX_NONE;
	TArray<FRunResult> Runs;
	FString CsvPath;
	FTSTicker::FDelegateHandle TickerHandle;
};

/** Blueprint access to FZEDMotionFpsMeter, e.g. to start a run from a UI button. */
UCLASS()
class ZEDLIVELINK_API UZEDMotionFpsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Starts the next 30 s motion FPS run. */
	UFUNCTION(BlueprintCallable, Category = "ZED|Motion FPS")
	static void StartMotionFpsRun();

	/** Clears the motion FPS runs. */
	UFUNCTION(BlueprintCallable, Category = "ZED|Motion FPS")
	static void ResetMotionFps();

	UFUNCTION(BlueprintPure, Category = "ZED|Motion FPS")
	static bool IsMotionFpsRunning();

	UFUNCTION(BlueprintPure, Category = "ZED|Motion FPS")
	static int32 GetMotionFpsCompletedRuns();

	/** Average FPS of the completed runs (0 if none). */
	UFUNCTION(BlueprintPure, Category = "ZED|Motion FPS")
	static float GetMotionFpsAverage();
};
