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
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WindowingFunctions.h"
#include <numeric>
#include "ModuloSameSignAsDivisor.h"

//==============================================================================
GamelanizerAudioProcessor::GamelanizerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::mono(), true)
                     .withOutput("Stereo Output", AudioChannelSet::stereo(), true)
                     .withOutput("Individual Out", AudioChannelSet::canonicalChannelSet(5), false)
      ),
      audioProcessorValueTreeState(*this, nullptr, Identifier("GamelanizerParameters"),
                                   gamelanizerParameters.createParameterLayout()),
      gamelanizerParametersVtsHelper(audioProcessorValueTreeState, gamelanizerParameters)
{
    currentBpm.store(120);

    jassert(currentBpm.is_lock_free());
    jassert(preventGuiBpmChange.is_lock_free());
}

//==============================================================================
void GamelanizerAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
#if MeasurePerformance
    performanceMeasures.reset();
#endif
    for (auto& subdivisionLevel : subdivisionLevels)
        subdivisionLevel.preparePhaseVocoder();

    const auto maxSamplesPerBeat = static_cast<int>(std::ceil(sampleRate * (60.0 / GamelanizerConstants::minBpm)));

    // maxLatency could be slightly smaller depending on initWriteHeadsAndLatencyMethod but this should always be enough
    const auto maxLatency = (maxSamplesPerBeat * 3) + 1;
    baseDelayBuffer.data.setSize(1, maxLatency);
    baseDelayBuffer.data.clear();

    levelsOutputBuffer.data.setSize(GamelanizerConstants::maxLevels, maxSamplesPerBeat * 4);
    levelsOutputBuffer.data.clear();

    gamelanizerParametersVtsHelper.resetSmoothers(sampleRate);

    hostSampleRate = sampleRate;

    prepareSamplesPerBeat();

    for (auto& sl : subdivisionLevels)
        sl.prepareFilters(samplesPerBlock);
}

void GamelanizerAudioProcessor::reset()
{
    for (auto& sl : subdivisionLevels)
        sl.resetFilters();

#if MeasurePerformance
    performanceMeasures.reset();
#endif
}

//==============================================================================
void GamelanizerAudioProcessor::setCurrentBpm(float newBpm)
{
    jassert(!hostIsPlaying);
    newBpm = jmin(newBpm, GamelanizerConstants::maxBpm);
    newBpm = jmax(newBpm, GamelanizerConstants::minBpm);
    currentBpm.store(newBpm);
    prepareSamplesPerBeat();
}

//==============================================================================
bool GamelanizerAudioProcessor::handleNotPlaying(const AudioPlayHead::CurrentPositionInfo& cpi)
{
    if (!cpi.isPlaying)
    {
        // if the DAW says it's not playing, but our internal state says we are
        if (hostIsPlaying)
        {
            // set the internal state to match the DAW
            hostIsPlaying = false;
            preventGuiBpmChange.store(false);
            // clear out the data that was written to the internal buffers
            baseDelayBuffer.data.clear();
            levelsOutputBuffer.data.clear();
        }
        // since the host isn't playing let the calling method know to return early
        return true;
    }
    // let the calling method know NOT to return early
    return false;
}

void GamelanizerAudioProcessor::handleTimelineJump(const int64 hostTimeInSamples)
{
    if (hostIsPlaying && hostTimeInSamples != hostSampleOughtToBe)
    {
        // if hostIsPlaying but hostSamples != hostSampleOughtToBe, that means the user jumped on the timeline
        // so clear the buffers
        baseDelayBuffer.data.clear();
        levelsOutputBuffer.data.clear();
    }
}

void GamelanizerAudioProcessor::simulateProcessing(const int64 hostTimeInSamples)
{
    processSamples(hostTimeInSamples, nullptr, nullptr, nullptr, nullptr, true);
}

bool GamelanizerAudioProcessor::handleStandaloneApp() const
{
    if (!hostIsPlaying)
    {
        // we should only get here if the plugin is running standalone
        if (JUCEApplication::isStandaloneApp())
            return true;
        DBG("no playhead and hostIsPlaying==false");
    }
    else
    {
        DBG("no playhead and hostIsPlaying==true");
    }
    // we shouldn't ever get here            
    jassert(false);
    return false;
}

bool GamelanizerAudioProcessor::handleTimelineStateChange()
{
    if (auto* ph = getPlayHead())
    {
        AudioPlayHead::CurrentPositionInfo cpi; // NOLINT(cppcoreguidelines-pro-type-member-init, hicpp-member-init)
        // if the host gives us play head information
        if (ph->getCurrentPosition(cpi))
        {
            // if we're not playing, return early
            if (handleNotPlaying(cpi)) return true;

            const auto hostTimeInSamples = cpi.timeInSamples;
            // if the host wasn't playing but now it is, or if we jumped around             
            if (!hostIsPlaying || hostTimeInSamples != hostSampleOughtToBe)
            {
                handleTimelineJump(hostTimeInSamples);

                hostIsPlaying = true;
                preventGuiBpmChange.store(true);

                // restart beatSampleInfo
                beatSampleInfo.reset();
                initLevelNoteSampleLengths();
                initWritePositions();
                // restart the internal host sample position
                hostSampleOughtToBe = 0;
                // zero the subdivision levels buffer read position
                levelsOutputBuffer.readPosition = 0;
                // zero the base delay buffer write position                
                initDlyReadPos();

                for (auto& sl : subdivisionLevels)
                    sl.fullReset();

                // without actually processing, get the state up to where it would be if the number of hostSamples had been processed
                simulateProcessing(hostTimeInSamples);
            }
        }
        else
        {
            jassert(false);
        }
    }
    else if (handleStandaloneApp()) return true;
    return false;
}

//==============================================================================
void GamelanizerAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& /*midiMessages*/)
{
    ScopedNoDenormals noDenormals;

    gamelanizerParametersVtsHelper.updateSmoothers();

    for (auto& sl : subdivisionLevels)
        sl.snapFiltersToZero();

    // initialization of member fields or return early if not playing
    if (handleTimelineStateChange()) return;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    const auto numSamples = buffer.getNumSamples();

    // clear the garbage after the number of input channels 
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // actual processing	
    auto* monoInputRead = buffer.getReadPointer(0);
    auto* multiOutWrite = buffer.getArrayOfWritePointers();
    auto* baseDelayBufferWrite = baseDelayBuffer.data.getWritePointer(0);
    auto* levelsBufferWrite = levelsOutputBuffer.data.getArrayOfWritePointers();
    processSamples(numSamples, monoInputRead, multiOutWrite, baseDelayBufferWrite, levelsBufferWrite, false);
}

void GamelanizerAudioProcessor::processSamples(const int64 numSamples, const float* monoInputRead,
                                               float** multiOutWrite, float* baseDelayBufferReadWrite,
                                               float** levelsBufferReadWrite, const bool skipProcessing)
{
#if MeasurePerformance
    const auto startingTime = PerformanceMeasures::getNewStartingTime();
#endif
    const auto levelOutBufferLength = levelsOutputBuffer.data.getNumSamples();
    const auto baseDelayBufferLength = baseDelayBuffer.data.getNumSamples();
    for (auto sample = 0; sample < numSamples; ++sample)
    {
        if (!skipProcessing)
        {
            const auto sampleData = monoInputRead[sample];

            // store new input sample into delay buffer
            baseDelayBufferReadWrite[baseDelayBuffer.writePosition] = sampleData;

            // have to apply gain changes here to get it to sync with automation
            const auto baseGain = gamelanizerParametersVtsHelper.getGain(0)
                * (1.0f - gamelanizerParametersVtsHelper.getMute(0));
            const auto baseOutput = baseDelayBufferReadWrite[baseDelayBuffer.readPosition] * baseGain;
            const auto basePanAmplitude = gamelanizerParametersVtsHelper.getPan(0) / 200.0f + 0.5f;

            // base - stereo out
            multiOutWrite[0][sample] = std::sqrt(1.0f - basePanAmplitude) * baseOutput;
            multiOutWrite[1][sample] = std::sqrt(basePanAmplitude) * baseOutput;

            // base - individual out
            if (getTotalNumOutputChannels() > 3)
                multiOutWrite[2][sample] = baseOutput;

            // subdivision levels
            for (auto level = 0; level < GamelanizerConstants::maxLevels; ++level)
            {
                // +1 is because base level is stored in this too
                const auto levelGain = gamelanizerParametersVtsHelper.getGain(level + 1)
                    * (1.0f - gamelanizerParametersVtsHelper.getMute(level + 1));

                const auto levelOutput = levelsBufferReadWrite[level][levelsOutputBuffer.readPosition] * levelGain;

                // erase head so the circular buffer can overlap when it wraps around 
                levelsBufferReadWrite[level][levelsOutputBuffer.readPosition] = 0.0f;

                subdivisionLevels[level].updateFilters();

                auto levelOutputFiltered = subdivisionLevels[level].lpFilter.processSample(levelOutput);
                levelOutputFiltered = subdivisionLevels[level].hpFilter.processSample(levelOutputFiltered);
                const auto levelPanAmplitude = gamelanizerParametersVtsHelper.getPan(level + 1) / 200.0f + 0.5f;
                // stereo out
                multiOutWrite[0][sample] += std::sqrt(1.0f - levelPanAmplitude) * levelOutputFiltered;
                multiOutWrite[1][sample] += std::sqrt(levelPanAmplitude) * levelOutputFiltered;

                // individual out (+3 is to skip the stereo out and base channels)
                if (level + 3 < getTotalNumOutputChannels())
                    multiOutWrite[level + 3][sample] = levelOutputFiltered;

                // =========================

                subdivisionLevels[level].queuePhaseVocoderNextParams();

                const auto taperAlpha = gamelanizerParametersVtsHelper.getTaper(level);
                const auto taperedSampleData = sampleData * WindowingFunctions::tukeyWindow(
                    beatSampleInfo.getSamplesIntoBeat(), beatSampleInfo.getBeatSampleLength(), taperAlpha);
                subdivisionLevels[level].processSample(taperedSampleData);
            }
        }

        // if we're on a beat boundary
        if (beatSampleInfo.isPastBeatEnd())
            nextBeat();
        else
            beatSampleInfo.incrementSamplesIntoBeat();

        // update indices and circle them back around if necessary
        ++levelsOutputBuffer.readPosition;
        if (levelsOutputBuffer.readPosition == levelOutBufferLength)
            levelsOutputBuffer.readPosition = 0;

        ++baseDelayBuffer.writePosition;
        if (baseDelayBuffer.writePosition == baseDelayBufferLength)
            baseDelayBuffer.writePosition = 0;

        ++baseDelayBuffer.readPosition;
        if (baseDelayBuffer.readPosition == baseDelayBufferLength)
            baseDelayBuffer.readPosition = 0;

        ++hostSampleOughtToBe;
    }
#if MeasurePerformance
    performanceMeasures.finishMeasurements(startingTime, hostSampleOughtToBe);
#endif
}

//==============================================================================

void GamelanizerAudioProcessor::nextBeat()
{
    for (auto& sl : subdivisionLevels)
    {
        sl.processFinalHop();
        sl.fastForwardWriteHeadsToNextBeat();
        sl.pv.resetBetweenBeats();
        if (beatSampleInfo.isBeatB())
            sl.moveWritePosOnBeatB();
    }

    beatSampleInfo.setNextBeatInfo();
}

//==============================================================================
int GamelanizerAudioProcessor::calculateLatencyNeeded()
{
    switch (initWriteHeadsAndLatencyMethod)
    {
    case threeBeats:
        return static_cast<int>(std::ceil(3 * samplesPerBeatFractional));
    case earliestAWithC:
        {
            auto sum = static_cast<int>(std::ceil(2 * samplesPerBeatFractional));
            for (auto& subdivisionLevel : subdivisionLevels)
            {
                sum += subdivisionLevel.noteLengthInSamples;
            }
            return sum;
        }
    case earliestABeforeC:
        {
            auto sum = static_cast<int>(std::ceil(2 * samplesPerBeatFractional));
            for (auto j = 0; j < GamelanizerConstants::maxLevels - 1; ++j)
            {
                sum += subdivisionLevels[j].noteLengthInSamples;
            }
            return sum;
        }
    default:
        jassert(false);
        return 0;
    }
}

void GamelanizerAudioProcessor::initDlyReadPos()
{
    const auto delayTime = calculateLatencyNeeded();

    const auto dlyOutReadPosUnwrapped = baseDelayBuffer.writePosition - delayTime;
    const auto delayBufferLength = baseDelayBuffer.data.getNumSamples();
    jassert(delayTime < delayBufferLength);

    // wrap the read position
    baseDelayBuffer.readPosition = ModuloSameSignAsDivisor::mod(dlyOutReadPosUnwrapped, delayBufferLength);

    // TODO it would be better to call this only in prepareToPlay, with a maximum, and then set another delay here
    setLatencySamples(delayTime);
}

void GamelanizerAudioProcessor::initWritePositions()
{
    // TODO subsample
    const auto twoBeats = static_cast<int>(std::round(samplesPerBeatFractional * 2));
    switch (initWriteHeadsAndLatencyMethod)
    {
    case threeBeats:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                subdivisionLevels[i].writePosition = twoBeats + subdivisionLevels.back().noteLengthInSamples;
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    subdivisionLevels[i].writePosition += subdivisionLevels[j].noteLengthInSamples;
            }
            break;
        }
    case earliestAWithC:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                subdivisionLevels[i].writePosition = twoBeats;
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    subdivisionLevels[i].writePosition += subdivisionLevels[j].noteLengthInSamples;
            }
            break;
        }
    case earliestABeforeC:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                subdivisionLevels[i].writePosition = twoBeats - subdivisionLevels.back().noteLengthInSamples;
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    subdivisionLevels[i].writePosition += subdivisionLevels[j].noteLengthInSamples;
            }
            break;
        }
    default:
        jassert(false);
        break;
    }
}

//==============================================================================

void GamelanizerAudioProcessor::initLevelNoteSampleLengths()
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
    {
        const auto noteLength = samplesPerBeatFractional * (1.0 / pow(2, i + 1));
        subdivisionLevels[i].noteLengthInSamplesFractional = noteLength;
        subdivisionLevels[i].noteLengthInSamples = static_cast<int>(std::round(noteLength));
    }
}

void GamelanizerAudioProcessor::prepareSamplesPerBeat()
{
    const auto currentBpmLocal = currentBpm.load();
    samplesPerBeatFractional = hostSampleRate * (60.0 / currentBpmLocal);
    beatSampleInfo.reset(samplesPerBeatFractional);
}

//==============================================================================
void GamelanizerAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    const auto state = audioProcessorValueTreeState.copyState();
    const auto xml(state.createXml());
    xml->setAttribute("currentBpm", static_cast<double>(currentBpm.load()));
    copyXmlToBinary(*xml, destData);
}

void GamelanizerAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    const auto xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasAttribute("currentBpm"))
        {
            currentBpm.store(static_cast<float>(xmlState->getDoubleAttribute("currentBpm", 120.0)));
            xmlState->removeAttribute("currentBpm");
        }
        if (xmlState->hasTagName(audioProcessorValueTreeState.state.getType()))
        {
            const auto newState = ValueTree::fromXml(*xmlState);
            audioProcessorValueTreeState.replaceState(newState);
            // because #setStateInformation will be called after #gamelanizerParametersVtsHelper is constructed, 
            // we need to force the smoothers to be at the correct values again
            gamelanizerParametersVtsHelper.instantlyUpdateSmoothers();
            // and then queue those new pitch shift parameters so they'll take effect at the beginning of playback

            for (auto& sl : subdivisionLevels)
                sl.queuePhaseVocoderNextParams();
        }
    }
}

//==============================================================================

AudioProcessorEditor* GamelanizerAudioProcessor::createEditor()
{
    //return new GenericAudioProcessorEditor(this);
    return new GamelanizerAudioProcessorEditor(*this, audioProcessorValueTreeState, gamelanizerParameters);
}

bool GamelanizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inBuses = layouts.inputBuses;
    const auto outBuses = layouts.outputBuses;
    if (inBuses.size() == 1)
    {
        const auto inBus0Size = inBuses[0].size();
        if (outBuses.size() == 1)
        {
            const auto outBus0Size = outBuses[0].size();

            if (inBus0Size == 1 && outBus0Size == 2)
                return true;
        }
        if (outBuses.size() == 2)
        {
            const auto outBus0Size = outBuses[0].size();
            const auto outBus1Size = outBuses[1].size();

            if (inBus0Size == 1 && outBus0Size == 2
                && (outBus1Size == GamelanizerConstants::maxLevels + 1 || outBus1Size == 0))
                return true;
        }
    }
    return false;
}

//============================================================================== 

/**
 * \brief Creates a new instances of the plugin.
 */
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GamelanizerAudioProcessor(); }
