
//==============================================================================
Knob::Knob (Parameter* p, bool fromCentre)
  : ParamComponent (p),
    value (parameter),
    knob (parameter, juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
    internalKnobReduction(3)
{
    addAndMakeVisible (name);
    addAndMakeVisible (value);
    addAndMakeVisible (knob);
    addChildComponent (modDepthSlider);

    if(p->getTooltip().isNotEmpty())
    {
        getSlider().setTooltip(p->getTooltip());
    }

    modDepthSlider.setRange (-1.0, 1.0, 0.0);
    modDepthSlider.setPopupDisplayEnabled (true, true, findParentComponentOfClass<juce::AudioProcessorEditor>());
    modDepthSlider.setDoubleClickReturnValue (true, 0.0);

    knob.setTitle (parameter->getName (100));
    knob.setDoubleClickReturnValue (true, parameter->getUserDefaultValue());
    knob.setSkewFactor (parameter->getSkew(), parameter->isSkewSymmetric());
    if (fromCentre)
        knob.getProperties().set ("fromCentre", true);

    knob.setName (parameter->getShortName());

    name.setText (parameter->getShortName(), juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
   #if JUCE_IOS
    knob.setMouseDragSensitivity (500);
   #endif

    value.setTitle (parameter->getName (100));
    value.setJustificationType (juce::Justification::centred);
    value.setFont(value.getFont().withPointHeight(14));
    value.setVisible (false);

    addMouseListener (this, true);

    if (parameter->getModIndex() >= 0)
    {
        auto& mm = *parameter->getModMatrix();
        mm.addListener (this);
    }

    modTimer.onTimer = [this] ()
    {
        auto& mm = *parameter->getModMatrix();
        if (mm.shouldShowLiveModValues())
        {
            auto curModValues = liveValuesCallback ? liveValuesCallback() : mm.getLiveValues (parameter, 0);
            if (curModValues != modValues)
            {
                modValues = curModValues;

                juce::Array<juce::var> vals;
                for (auto v : modValues)
                    vals.add (v);

                knob.getProperties().set ("modValues", vals);

                repaint();
            }

            //do the same thing, but for the right channel. To keep backwards compatability we don't rename modValues, or mess with it
            auto curModValuesR = liveValuesCallback ? liveValuesCallback() : mm.getLiveValues (parameter, 1);
            if (curModValuesR != modValuesR)
            {
                modValuesR = curModValuesR;

                juce::Array<juce::var> vals;
                for (auto v : modValuesR)
                    vals.add (v);

                knob.getProperties().set ("modValuesR", vals);

                repaint();
            }
        }
        else if (knob.getProperties().contains ("modValues"))
        {
            knob.getProperties().remove ("modValues");
            knob.getProperties().remove("modValuesR");
            repaint();
        }
    };
    shiftTimer.onTimer = [this] ()
    {
        bool shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
        knob.setInterceptsMouseClicks (! learning || shift, ! learning || shift );
    };

    modDepthSlider.onClick = [this] { showModMenu(); };
    modDepthSlider.setMouseDragSensitivity (500);
    modDepthSlider.onValueChange = [this]
    {
        if (auto mm = parameter->getModMatrix())
        {
            auto dst = ModDstId (parameter->getModIndex());

            if (auto depths = mm->getModDepths (dst); depths.size() > 0)
            {
                auto range = parameter->getUserRange();
                if (range.interval <= 0.0f || juce::ModifierKeys::currentModifiers.isShiftDown())
                {
                    mm->setModDepth (depths[0].first, dst, float (modDepthSlider.getValue()));
                }
                else
                {
                    auto uv = range.convertFrom0to1 (std::clamp (float (parameter->getValue() + modDepthSlider.getValue()), 0.0f, 1.0f));
                    auto nv = range.convertTo0to1 (range.snapToLegalValue (uv));

                    auto d = nv - parameter->getValue();

                    mm->setModDepth (depths[0].first, dst, d);
                    modDepthSlider.setValue (d, juce::dontSendNotification);
                }
            }
        }
    };
    modDepthSlider.onTextFromValue = [this] (double v)
    {
        if (auto mm = parameter->getModMatrix())
        {
            auto dst = ModDstId (parameter->getModIndex());

            if (auto depths = mm->getModDepths (dst); depths.size() > 0)
            {
                auto d = depths[0];

                auto pname      = mm->getModSrcName (d.first);
                auto val        = parameter->getText (std::clamp (float (parameter->getValue() + v), 0.0f, 1.0f), 1000);

                return pname + ": " + val + " " + parameter->getLabel();
            }
        }
        return juce::String();
    };
    modMatrixChanged();
}

Knob::~Knob()
{
    if (parameter->getModIndex() >= 0)
    {
        auto& mm = *parameter->getModMatrix();
        mm.removeListener (this);
    }
}

void Knob::setDisplayName (const juce::String& n)
{
    name.setText (n, juce::dontSendNotification);
}

void Knob::showModMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());

    auto& mm = *parameter->getModMatrix();
    for (auto src : mm.getModSources (parameter))
    {
        m.addItem ("Remove: " + mm.getModSrcName (src), [this, src]
        {
            parameter->getModMatrix()->clearModDepth (src, ModDstId (parameter->getModIndex()));
        });
    }

    m.showMenuAsync ({});
}

void Knob::paint (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (findColour (PluginLookAndFeel::accentColourId, true).withAlpha (0.3f));
        g.fillEllipse (knob.getBounds().toFloat());
    }
}

void Knob::resized()
{
    auto r = getLocalBounds();

    auto extra = r.getHeight() - r.getWidth();

    auto rc = r.removeFromBottom (extra);

    name.setBounds (rc);
    value.setBounds (rc);
    knob.setBounds (r.reduced (internalKnobReduction + 2));

    modDepthSlider.setBounds (knob.getBounds().removeFromTop (7).removeFromRight (7).reduced (-3));
}

void Knob::mouseEnter (const juce::MouseEvent&)
{
    if (wantsAccessibleKeyboard (*this))
        return;

    if (! isTimerRunning() && isEnabled())
    {
        startTimer (100);
        name.setVisible (false);
        value.setVisible (true);
    }
}

void Knob::timerCallback()
{
    auto p = getMouseXYRelative();
    if (! getLocalBounds().contains (p) &&
        ! juce::ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown() &&
        ! value.isBeingEdited())
    {
        if (wantsAccessibleKeyboard (*this))
        {
            name.setVisible (false);
            value.setVisible (true);
        }
        else
        {
            name.setVisible (true);
            value.setVisible (false);
        }

        stopTimer();
    }
}

void Knob::parentHierarchyChanged()
{
    auto a = wantsAccessibleKeyboard (*this);
    name.setWantsKeyboardFocus (a);
    value.setWantsKeyboardFocus (a);
    knob.setWantsKeyboardFocus (a);

    if (wantsAccessibleKeyboard (*this))
    {
        name.setVisible (false);
        value.setVisible (true);
    }
    else
    {
        name.setVisible (true);
        value.setVisible (false);
    }
}

void Knob::learnSourceChanged (ModSrcId src)
{
    learning = src.isValid();

    bool shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
    knob.setInterceptsMouseClicks (! learning || shift, ! learning || shift );

    auto& mm = *parameter->getModMatrix();
    modDepth = mm.getModDepth (mm.getLearn(), ModDstId (parameter->getModIndex()));

    if (learning)
    {
        knob.getProperties().set ("modDepth", modDepth);
        knob.getProperties().set ("modBipolar", mm.getModBipolarMapping (mm.getLearn(), ModDstId (parameter->getModIndex())));

        shiftTimer.startTimerHz (100);
    }
    else
    {
        knob.getProperties().remove ("modDepth");
        knob.getProperties().remove ("modBipolar");

        shiftTimer.stopTimer();
    }

    repaint();
}

void Knob::modMatrixChanged()
{
    if (auto mm = parameter->getModMatrix())
    {
        auto dst = ModDstId (parameter->getModIndex());

        if (mm->isModulated (dst) || liveValuesCallback)
        {
            modTimer.startTimerHz (30);

            auto vis = mm->isModulated (dst);
            if (vis != modDepthSlider.isVisible())
            {
                modDepthSlider.setVisible (vis);
                resized();
            }

            if (auto depths = mm->getModDepths (dst); depths.size() > 0)
                modDepthSlider.setValue (depths[0].second, juce::dontSendNotification);
            else
                modDepthSlider.setValue (0.0f, juce::dontSendNotification);
        }
        else
        {
            modTimer.stopTimer();
            knob.getProperties().remove ("modValues");

            if (modDepthSlider.isVisible())
            {
                modDepthSlider.setVisible (false);
                resized();
            }
        }

        if (learning && ! isMouseButtonDown (true))
        {
            modDepth = mm->getModDepth (mm->getLearn(), dst);
            knob.getProperties().set ("modDepth", modDepth);
            knob.getProperties().set ("modBipolar", mm->getModBipolarMapping (mm->getLearn(), ModDstId (parameter->getModIndex())));
            repaint();
        }
    }
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    bool shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
    if (shift || ! learning || ! knob.getBounds().contains (e.getMouseDownPosition()))
        return;

    auto& mm = *parameter->getModMatrix();
    auto dst = ModDstId (parameter->getModIndex());
    modDepth = mm.getModDepth (mm.getLearn(), dst);

    knob.getProperties().set ("modDepth", modDepth);

    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;

    bool shift = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
    if (shift || ! learning || ! knob.getBounds().contains (e.getMouseDownPosition()))
         return;

    if (e.getDistanceFromDragStart() >= 3)
    {
        auto pt = e.getMouseDownPosition();
        auto delta = (e.position.x - pt.getX()) + (pt.getY() - e.position.y);

        float newModDepth = juce::jlimit (-1.0f, 1.0f, delta / 200.0f + modDepth);

        knob.getProperties().set ("modDepth", newModDepth);

        auto& mm = *parameter->getModMatrix();
        auto dst = ModDstId (parameter->getModIndex());

        auto range = parameter->getUserRange();
        if (range.interval <= 0.0f || juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            mm.setModDepth (mm.getLearn(), dst, float (modDepthSlider.getValue()));
        }
        else
        {
            auto uv = range.convertFrom0to1 (std::clamp (float (parameter->getValue() + modDepthSlider.getValue()), 0.0f, 1.0f));
            auto nv = range.convertTo0to1 (range.snapToLegalValue (uv));

            auto d = nv - parameter->getValue();

            mm.setModDepth (mm.getLearn(), dst, d);
            modDepthSlider.setValue (d, juce::dontSendNotification);
        }

        repaint();
    }
}

bool Knob::isInterestedInDragSource (const SourceDetails& sd)
{
    if (isEnabled() && parameter && parameter->getModMatrix())
        return sd.description.toString().startsWith ("modSrc");

    return false;
}

void Knob::itemDragEnter (const SourceDetails&)
{
    dragOver = true;
    repaint();
}

void Knob::itemDragExit (const SourceDetails&)
{
    dragOver = false;
    repaint();
}

void Knob::itemDropped (const SourceDetails& sd)
{
    dragOver = false;
    repaint();

    auto& mm = *parameter->getModMatrix();

    auto src = ModSrcId (sd.description.toString().getTrailingIntValue());
    auto dst = ModDstId (parameter->getModIndex());

    mm.setModDepth (src, dst, 1.0f);
}
