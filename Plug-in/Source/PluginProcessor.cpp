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
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        hpFilters[i].parameters->type = dsp::StateVariableFilter::Parameters<float>::Type::highPass;

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        levelPowers[i] = static_cast<int>(std::pow(2, i + 1));

    currentBpm.store(120);

    jassert(currentBpm.is_lock_free());
    jassert(hostIsPlaying.is_lock_free());
}

GamelanizerAudioProcessor::~GamelanizerAudioProcessor()
{
}


//==============================================================================
void GamelanizerAudioProcessor::prepareToPlay(const double sampleRate, const int samplesPerBlock)
{
#if MeasurePerformance
    performanceMeasures.reset();
#endif
    initAllPhaseVocoders();

    const auto maxSamplesPerBeat = static_cast<int>(std::ceil(sampleRate * (60.0 / GamelanizerConstants::minBpm)));

    // maxLatency could be slightly smaller depending on initWriteHeadsAndLatencyMethod but this should always be enough
    const auto maxLatency = (maxSamplesPerBeat * 3) + 1;
    baseDelayBuffer.data.setSize(1, maxLatency);
    baseDelayBuffer.data.clear();

    levelsOutputBuffer.data.setSize(GamelanizerConstants::maxLevels, maxSamplesPerBeat * 4);
    levelsOutputBuffer.data.clear();

    gamelanizerParametersVtsHelper.resetSmoothers(sampleRate);

    prepareFilters(sampleRate, samplesPerBlock);

    hostSampleRate = sampleRate;
    nyquist = static_cast<float>(hostSampleRate * 0.5f);
    prepareSamplesPerBeat();
}

void GamelanizerAudioProcessor::reset()
{
    for (auto& filter : lpFilters)
    {
        filter.reset();
    }
    for (auto& filter : hpFilters)
    {
        filter.reset();
    }
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
void GamelanizerAudioProcessor::initAllPhaseVocoders()
{
    for (auto level = 0; level < GamelanizerConstants::maxLevels; ++level)
    {
        const auto pitchParam = gamelanizerParametersVtsHelper.getPitch(level, false);
        const auto pitchShiftFactor = std::pow(2.0, pitchParam.value / 1200.0);
        const auto timeScaleFactor = 1.0 / levelPowers[level];
        pvs[level].initParams(static_cast<float>(pitchShiftFactor), static_cast<float>(timeScaleFactor));
    }
}

void GamelanizerAudioProcessor::queuePhaseVocoderNextParams(const int level)
{
    const auto newPitch = gamelanizerParametersVtsHelper.getPitch(level, true);
    if (newPitch.wasChanged)
    {
        pvs[level].queueParams(newPitch.value);
    }
}

//==============================================================================
bool GamelanizerAudioProcessor::handleNotPlaying(const AudioPlayHead::CurrentPositionInfo& cpi)
{
    if (!cpi.isPlaying)
    {
        // if the DAW says it's not playing, but our internal state says we are
        if (hostIsPlaying.load())
        {
            // set the internal state to match the DAW
            hostIsPlaying.store(false);
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

void GamelanizerAudioProcessor::handleTimelineJump(const int64 hostTimeInSamples, const bool hostIsPlayingLocal)
{
    if (hostIsPlayingLocal && hostTimeInSamples != hostSampleOughtToBe)
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
    if (!hostIsPlaying.load())
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
        AudioPlayHead::CurrentPositionInfo cpi;
        // if the host gives us play head information
        if (ph->getCurrentPosition(cpi))
        {
            // if we're not playing, return early
            if (handleNotPlaying(cpi)) return true;

            const auto hostTimeInSamples = cpi.timeInSamples;
            const auto hostIsPlayingLocal = hostIsPlaying.load();
            // if the host wasn't playing but now it is, or if we jumped around             
            if (!hostIsPlayingLocal || hostTimeInSamples != hostSampleOughtToBe)
            {
                handleTimelineJump(hostTimeInSamples, hostIsPlayingLocal);

                // reset/update the internal state
                hostIsPlaying.store(true);
                hostSampleOughtToBe = 0;
                levelsOutputBuffer.readPosition = 0;
                beatSampleInfo.reset(samplesPerBeatFractional);
                initLevelNoteSampleLengths();
                initWritePositions();
                baseDelayBuffer.writePosition = 0;
                initDlyReadPos();

                for (auto& pv : pvs)
                    pv.fullReset();

                for (auto& levelAccumulatedSample : levelAccumulatedSamples)
                    levelAccumulatedSample = 0;

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
    snapFiltersToZero();

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
    auto* monoDelayBufferWrite = baseDelayBuffer.data.getWritePointer(0);
    auto* outputBufferWrite = levelsOutputBuffer.data.getArrayOfWritePointers();
    auto* multiOutWrite = buffer.getArrayOfWritePointers();
    processSamples(numSamples, monoInputRead, monoDelayBufferWrite, multiOutWrite, outputBufferWrite, false);
}

void GamelanizerAudioProcessor::updateLpFilter(const int level)
{
    const auto lpFilterCutoff = gamelanizerParametersVtsHelper.getLpFilterCutoff(level);
    if (lpFilterCutoff.wasChanged)
        lpFilters[level].parameters->setCutOffFrequency(hostSampleRate, jmin(nyquist, lpFilterCutoff.value));
}

void GamelanizerAudioProcessor::updateHpFilter(const int level)
{
    const auto hpFilterCutoff = gamelanizerParametersVtsHelper.getHpFilterCutoff(level);
    if (hpFilterCutoff.wasChanged)
        hpFilters[level].parameters->setCutOffFrequency(hostSampleRate, jmin(nyquist, hpFilterCutoff.value));
}

void GamelanizerAudioProcessor::processSamples(const int64 numSamples, const float* baseDelayBufferWrite,
                                               float* dlyBufferData,
                                               float** multiOutWrite, float** outputBufferWrite,
                                               const bool skipProcessing)
{
#if MeasurePerformance
    const auto startingTime = performanceMeasures.getNewStartingTime();
#endif
    for (auto sample = 0; sample < numSamples; ++sample)
    {
        if (skipProcessing)
        {
            for (auto level = 0; level < GamelanizerConstants::maxLevels; ++level)
                processSample(level, 0.0f, true);
        }
        else
        {
            // store new input sample into delay buffer
            dlyBufferData[baseDelayBuffer.writePosition] = baseDelayBufferWrite[sample];

            const auto sampleData = baseDelayBufferWrite[sample];

            // have to apply gain changes here to get it to sync with automation
            const auto baseGain = gamelanizerParametersVtsHelper.getGain(0)
                * (1.0f - gamelanizerParametersVtsHelper.getMute(0));
            const auto baseOutput = dlyBufferData[baseDelayBuffer.readPosition] * baseGain;
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
                const auto levelOutput = outputBufferWrite[level][levelsOutputBuffer.readPosition] * levelGain;

                updateLpFilter(level);
                updateHpFilter(level);

                auto levelOutputFiltered = lpFilters[level].processSample(levelOutput);
                levelOutputFiltered = hpFilters[level].processSample(levelOutputFiltered);
                const auto levelPanAmplitude = gamelanizerParametersVtsHelper.getPan(level + 1) / 200.0f + 0.5f;
                // stereo out
                multiOutWrite[0][sample] += std::sqrt(1.0f - levelPanAmplitude) * levelOutputFiltered;
                multiOutWrite[1][sample] += std::sqrt(levelPanAmplitude) * levelOutputFiltered;

                // individual out (+3 is to skip the stereo out and base channels)
                if (level + 3 < getTotalNumOutputChannels())
                    multiOutWrite[level + 3][sample] = levelOutputFiltered;

                // erase head so the circular buffer can overlap when it wraps around 
                outputBufferWrite[level][levelsOutputBuffer.readPosition] = 0.0f;

                // =========================

                queuePhaseVocoderNextParams(level);

                const auto taperAlpha = gamelanizerParametersVtsHelper.getTaper(level);
                const auto taperedSampleData = sampleData * WindowingFunctions::tukeyWindow(
                    beatSampleInfo.getSamplesIntoBeat(), beatSampleInfo.getBeatSampleLength(), taperAlpha);
                processSample(level, taperedSampleData, false);
            }
        }

        // if we're on a beat boundary
        if (beatSampleInfo.isPastBeatEnd())
            nextBeat();
        else
            beatSampleInfo.incrementSamplesIntoBeat();

        // update indices		
        ++levelsOutputBuffer.readPosition;
        ++baseDelayBuffer.writePosition;
        ++baseDelayBuffer.readPosition;
        // circle indices back around
        if (levelsOutputBuffer.readPosition == levelsOutputBuffer.data.getNumSamples())
            levelsOutputBuffer.readPosition = 0;

        if (baseDelayBuffer.writePosition == baseDelayBuffer.data.getNumSamples())
            baseDelayBuffer.writePosition = 0;

        if (baseDelayBuffer.readPosition == baseDelayBuffer.data.getNumSamples())
            baseDelayBuffer.readPosition = 0;

        ++hostSampleOughtToBe;
    }
#if MeasurePerformance
    performanceMeasures.finishMeasurements(startingTime, hostSampleOughtToBe);
#endif
}

void GamelanizerAudioProcessor::processSample(const int level, const float sampleValue,
                                              const bool skipProcessing)
{
    auto pv = &pvs[level];

    const auto hop = pv->processSample(sampleValue, skipProcessing);
    // hop returns greater than 0 when the phase vocoder has new data for us to OLA
    if (hop > 0)
    {
        if (!skipProcessing)
        {
            addSamplesToLevelsOutputBuffer(level, pv->getFftInOutReadPointer(), PhaseVocoder::getFftSize());
            pv->loadNextParams();
        }
        levelWritePositions[level] += hop;
        levelAccumulatedSamples[level] += hop;
    }
}

//==============================================================================
void GamelanizerAudioProcessor::addSamplesToLevelsOutputBuffer(const int level, const float* samples,
                                                         const int nSamples)
{
    const auto power = levelPowers[level];
    const auto noteLength = static_cast<double>(beatSampleInfo.getBeatSampleLength()) / power;
    const auto twoNoteLengths = noteLength * 2;

    const auto numCopies = power;

    for (auto i = 0; i < numCopies; ++i)
    {
        if (shouldDropThisNote(level, i))
            continue;

        const auto outBufLength = levelsOutputBuffer.data.getNumSamples();
        // multiple write heads for each copy of the scaled beat, depending on the subdivision lvl;
        const auto writeHead = (levelWritePositions[level] + static_cast<int>(twoNoteLengths * i)) % outBufLength;
        jassert(writeHead >= 0);
        jassert(writeHead < outBufLength);

        // if the samples array needs to wrap around the output buffer
        if (writeHead + nSamples > outBufLength)
        {
            // number of samples on the left part of the samples array
            const auto nSamplesLeft = outBufLength - writeHead;
            jassert(nSamplesLeft > 0);
            jassert(nSamplesLeft < nSamples);
            // make sure we aren't writing past the read head
            if (writeHead < levelsOutputBuffer.readPosition)
            {
                //TODO <= ?
                jassert(writeHead + nSamplesLeft < levelsOutputBuffer.readPosition);
            }
            levelsOutputBuffer.data.addFrom(level, writeHead, samples, nSamplesLeft);
            // number of samples on the right part of the samples array
            const auto nSamplesRight = nSamples - nSamplesLeft;
            jassert(nSamplesRight > 0);
            jassert(nSamplesRight < nSamples);
            // make sure we aren't writing past the read head
            jassert(nSamplesLeft + nSamplesRight < levelsOutputBuffer.readPosition);
            levelsOutputBuffer.data.addFrom(level, 0, samples + nSamplesLeft, nSamplesRight);
        }
        else
        {
            levelsOutputBuffer.data.addFrom(level, writeHead, samples, nSamples);
        }
    }
}

bool GamelanizerAudioProcessor::shouldDropThisNote(const int level, const int copyNumber)
{
    // drop the 1st note
    const auto onFirstNote = !beatSampleInfo.isBeatB() && (copyNumber % 2 == 0);
    if (onFirstNote && (gamelanizerParametersVtsHelper.getDropNote(level, 0) != 0))
        return true;

    // drop the 2nd note
    const auto onSecondNote = beatSampleInfo.isBeatB() && (copyNumber % 2 == 0);
    if (onSecondNote && (gamelanizerParametersVtsHelper.getDropNote(level, 1) != 0))
        return true;

    // drop the 3rd note
    const auto onThirdNote = !beatSampleInfo.isBeatB() && (copyNumber % 2 == 1);
    if (onThirdNote && (gamelanizerParametersVtsHelper.getDropNote(level, 2) != 0))
        return true;

    // drop the 4th note
    const auto onFourthNote = beatSampleInfo.isBeatB() && (copyNumber % 2 == 1);
    if (onFourthNote && (gamelanizerParametersVtsHelper.getDropNote(level, 3) != 0))
        return true;

    return false;
}

void GamelanizerAudioProcessor::moveWritePosOnBeatB()
{
    for (auto level = 0; level < GamelanizerConstants::maxLevels; ++level)
    {
        // we have to add 2 because we're starting from 0. If we counted the base level as 0, we would only add 1
        const auto numberNotesInThisLvlEqualToTwoInTheOriginal = pow(2, 2 + level);
        // by now, the lead write head is at the end of the 2nd note, so we don't have to jump past those 2 notes
        const auto numberOfNotesToJumpOver = numberNotesInThisLvlEqualToTwoInTheOriginal - 2;

        const auto beatLengthScaled = static_cast<double>(beatSampleInfo.getBeatSampleLength()) / levelPowers[level];
        // TODO sub sample?
        const auto samplesToJump = static_cast<int>(beatLengthScaled * numberOfNotesToJumpOver);
        levelWritePositions[level] += samplesToJump;
    }
}

void GamelanizerAudioProcessor::fastForwardWriteHeadsToNextBeat()
{
    for (auto level = 0; level < GamelanizerConstants::maxLevels; ++level)
    {
        const auto noteWidth = levelNoteSampleLengths[level];
        //TODO sub sample
        const auto missing = static_cast<int>(noteWidth - levelAccumulatedSamples[level]);
        levelWritePositions[level] += missing;
        levelAccumulatedSamples[level] = 0;
    }
}

void GamelanizerAudioProcessor::nextBeat()
{
    fastForwardWriteHeadsToNextBeat();

    //reset all phase vocoders
    for (auto& pv : pvs)
        pv.reset();

    if (beatSampleInfo.isBeatB())
        moveWritePosOnBeatB();

    beatSampleInfo.setNextBeatInfo(samplesPerBeatFractional);
}

//==============================================================================
int GamelanizerAudioProcessor::calculateLatencyNeeded()
{
    switch (initWriteHeadsAndLatencyMethod)
    {
    case threeBeats:
        return static_cast<int>(std::ceil(3 * samplesPerBeatFractional));
    case earliestAWithC:
        return static_cast<int>(std::ceil(std::accumulate(levelNoteSampleLengths.begin(),
                                                          levelNoteSampleLengths.end(),
                                                          2 * samplesPerBeatFractional)));
    case earliestABeforeC:
        return static_cast<int>(std::ceil(std::accumulate(levelNoteSampleLengths.begin(),
                                                          levelNoteSampleLengths.end() - 1,
                                                          2 * samplesPerBeatFractional)));
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
    baseDelayBuffer.readPosition = ModuloSameSignAsDivisor::modInt(dlyOutReadPosUnwrapped, delayBufferLength);

    // TODO it would be better to call this only in prepareToPlay, with a maximum, and then set another delay here
    setLatencySamples(delayTime);
}

void GamelanizerAudioProcessor::initWritePositions()
{
    // TODO sub sample rounding stuff
    const auto twoBeats = static_cast<int>(std::round(samplesPerBeatFractional * 2));
    switch (initWriteHeadsAndLatencyMethod)
    {
    case threeBeats:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                levelWritePositions[i] = twoBeats + levelNoteSampleLengths.back();
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    levelWritePositions[i] += levelNoteSampleLengths[j];
            }
            break;
        }
    case earliestAWithC:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                levelWritePositions[i] = twoBeats;
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    levelWritePositions[i] += levelNoteSampleLengths[j];
            }
            break;
        }
    case earliestABeforeC:
        {
            for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
            {
                levelWritePositions[i] = twoBeats - levelNoteSampleLengths.back();
                for (auto j = i + 1; j < GamelanizerConstants::maxLevels; j++)
                    levelWritePositions[i] += levelNoteSampleLengths[j];
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
    // TODO sub sample
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        levelNoteSampleLengths[i] = static_cast<int>(std::round(samplesPerBeatFractional * (1.0 / pow(2, i + 1))));
}

void GamelanizerAudioProcessor::prepareSamplesPerBeat()
{
    const auto currentBpmLocal = currentBpm.load();
    samplesPerBeatFractional = hostSampleRate * (60.0 / currentBpmLocal);
    beatSampleInfo.reset(samplesPerBeatFractional);
}

//==============================================================================

void GamelanizerAudioProcessor::prepareFilters(const double sampleRate, const int samplesPerBlock)
{
    for (auto& filter : lpFilters)
        filter.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(1)});

    for (auto& filter : hpFilters)
        filter.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), static_cast<uint32>(1)});
}

void GamelanizerAudioProcessor::snapFiltersToZero()
{
    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        lpFilters[i].snapToZero();

    for (auto i = 0; i < GamelanizerConstants::maxLevels; ++i)
        hpFilters[i].snapToZero();
}

//==============================================================================
void GamelanizerAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    const auto state = audioProcessorValueTreeState.copyState();
    const std::unique_ptr<XmlElement> xml(state.createXml());
    xml->setAttribute("currentBpm", static_cast<double>(currentBpm.load()));
    copyXmlToBinary(*xml, destData);
}

void GamelanizerAudioProcessor::setStateInformation(const void* data, const int sizeInBytes)
{
    const std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
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
