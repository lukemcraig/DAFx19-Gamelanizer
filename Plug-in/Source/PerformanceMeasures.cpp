/*
  ==============================================================================

	This file is part of Gamelanizer.
	Copyright (c) 2019 - Luke McDuffie Craig.

	Gamelanizer is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Gamelanizer is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Gamelanizer. If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/
#if MeasurePerformance
#include "PerformanceMeasures.h"

void PerformanceMeasures::reset()
{
    measurementIndex = 0;
    measurements = {};
    measurementSampleLocations = {};
}

std::chrono::steady_clock::time_point PerformanceMeasures::getNewStartingTime()
{
    return std::chrono::high_resolution_clock::now();
}

void PerformanceMeasures::finishMeasurements(
    const std::chrono::time_point<std::chrono::steady_clock> startingTime, const int64 sampleLocation)
{
    const auto end = std::chrono::high_resolution_clock::now();
    if (measurementIndex < measurements.size()
        && (measurementIndex == 0 || measurementSampleLocations[measurementIndex - 1] < stopAtSample))
    {
        measurements[measurementIndex] = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startingTime).
            count();
        measurementSampleLocations[measurementIndex] = sampleLocation;
        ++measurementIndex;
        if (measurementIndex == measurements.size() || measurementSampleLocations[measurementIndex - 1] >= stopAtSample)
        {
            const auto bufferSize = measurementSampleLocations[1] - measurementSampleLocations[0];
#ifdef JUCE_DEBUG
            auto filename = String("Debug");
#else
			auto filename = String("Release");
#endif
            filename = filename + String(bufferSize) + (JUCE_DSP_USE_INTEL_MKL ? "MKL" : "Fallback");

			// make a new measurement log file for each playback
            auto measurementLog{
                File::getSpecialLocation(File::SpecialLocationType::userDesktopDirectory)
                .getChildFile("GamelanizerLogs").getNonexistentChildFile(filename, ".csv", true)
            };

            if (measurementLog.exists())
                return;
            const auto created = measurementLog.create();

            if (created.wasOk())
            {
                if (measurementLog.getFullPathName().isNotEmpty())
                {
                    FileOutputStream out(measurementLog);
                    MemoryOutputStream s;
                    if (!out.failedToOpen())
                    {
                        for (size_t i = 0; i < measurements.size(); ++i)
                        {
                            s << String(measurementSampleLocations[i]) << "," << String(measurements[i]) << newLine;
                            if (measurementSampleLocations[i] >= stopAtSample)
                                break;
                        }
                        out << s.toString();
                    }
                }
            }
            else
            {
                DBG(created.getErrorMessage());
            }
        }
    }
}
#endif
