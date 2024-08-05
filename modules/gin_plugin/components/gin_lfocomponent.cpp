
LFOComponent::LFOComponent()
{
    setName ("lfo");
}

void LFOComponent::resized()
{
    dirty = true;
}

void LFOComponent::setParams (Parameter::Ptr wave_, Parameter::Ptr sync_, Parameter::Ptr rate_,
                              Parameter::Ptr beat_, Parameter::Ptr depth_, Parameter::Ptr offset_,
                              Parameter::Ptr phase_, Parameter::Ptr enable_, Parameter::Ptr stereo_)
{
    unwatchParams();

    watchParam (wave   = wave_);
    watchParam (sync   = sync_);
    watchParam (rate   = rate_);
    watchParam (beat   = beat_);
    watchParam (depth  = depth_);
    watchParam (offset = offset_);
    watchParam (phase  = phase_);
    watchParam (enable = enable_);
    watchParam (stereo = stereo_);

    startTimerHz (30);
}

void LFOComponent::paramChanged ()
{
    MultiParamComponent::paramChanged();
    phaseOffset = stereo->getProcValue();
    dirty = true;
}

void LFOComponent::createPath (juce::Rectangle<int> area)
{
    lfoL.setSampleRate ((double) area.getWidth());

    LFO::Parameters pL;
    pL.waveShape = (LFO::WaveShape) int (wave->getProcValue());
    pL.frequency = 1.0f * getNumSteps();
    pL.phase     = phase->getProcValue();
    pL.offset    = offset->getProcValue();
    pL.depth     = depth->getProcValue();

    LFO::Parameters pR = LFO::Parameters(pL);
    pR.phase += stereo->getProcValue();

    lfoL.setParameters (pL);
    lfoL.reset();

    lfoR.setParameters (pR);
    lfoR.reset();

    auto vToY = [&] (float v)
    {
        return area.getBottom() - (v + 1.0f) / 2.0f * area.getHeight();
    };

    curve.clear();
    path.clear();

    for (int x = area.getX(); x <= area.getRight(); x++)
    {
        auto v = lfoL.process (1);
        
        if (unclamped)
            v = lfoL.getOutputUnclamped();

        if (x == area.getX())
            path.startNewSubPath ({float (area.getX()), vToY (v)});
        else
            path.lineTo ({float (x), vToY (v)});

        curve[x - area.getX()] = vToY (v);
    }
}

void LFOComponent::paint (juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10);
    auto rc = getLocalBounds().reduced (2);

    if (dirty)
    {
        dirty = false;
        createPath (rc);
    }

    // g.setColour (dimIfNeeded (findColour (GinLookAndFeel::whiteColourId).withAlpha (0.3f)));
    // g.fillRect (rc.getX(), rc.getCentreY(), rc.getWidth(), 1);

    auto c = findColour (GinLookAndFeel::accentColourId).withAlpha (1.0f);

    g.setColour (dimIfNeeded (c));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    if (isEnabled())
    {
        auto lerp = [] (float t, float a, float b)  { return a + t * (b - a); };

        for (auto curPhase : curPhases)
        {
            float x = std::fmod (curPhase / getNumSteps(), 1.0f) * rc.getWidth();
            float t = x - int (x);
            float y = lerp (t, curve[int(x)], curve[int(x) + 1]);
            
            // g.setColour (dimIfNeeded (findColour (GinLookAndFeel::whiteColourId).withAlpha (0.9f)));
            g.setColour(juce::Colours::palevioletred.withLightness(0.7f).withAlpha(1.0f));
            g.fillEllipse (rc.getX() + x - 2, y - 2, 4, 4);

            x = std::fmod((curPhase + phaseOffset) / getNumSteps(), 1.0f) * rc.getWidth();
            t = x - int (x);
            y = lerp (t, curve[int(x)], curve[int(x) + 1]);

            g.setColour(juce::Colours::teal.withLightness(0.7f).withAlpha(1.0f));
            g.fillEllipse (rc.getX() + x - 2, y - 2, 4, 4);
        }
    }

    //draw a black rectangle around the background

}

void LFOComponent::timerCallback()
{
    if (isEnabled() && phaseCallback)
    {
        auto newPhases = phaseCallback();
        if (newPhases != curPhases)
        {
            curPhases = newPhases;
            repaint();
        }
    }
}

int LFOComponent::getNumSteps()
{
    LFO::WaveShape shape = (LFO::WaveShape) int (wave->getProcValue());

    if (shape == LFO::WaveShape::sampleAndHold) return 8;
    if (shape == LFO::WaveShape::noise)         return 8;

    return 1;
}
