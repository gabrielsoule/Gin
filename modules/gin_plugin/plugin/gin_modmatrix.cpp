//==============================================================================
ModMatrix::Function strToFunc (const juce::String& str)
{
    if (str == "linear") return ModMatrix::linear;
    if (str == "quadraticIn") return ModMatrix::quadraticIn;
    if (str == "quadraticInOut") return ModMatrix::quadraticInOut;
    if (str == "quadraticOut") return ModMatrix::quadraticOut;
    if (str == "sineIn") return ModMatrix::sineIn;
    if (str == "sineInOut") return ModMatrix::sineInOut;
    if (str == "sineOut") return ModMatrix::sineOut;
    if (str == "exponentialIn") return ModMatrix::exponentialIn;
    if (str == "exponentialInOut") return ModMatrix::exponentialInOut;
    if (str == "exponentialOut") return ModMatrix::exponentialOut;
    if (str == "invLinear") return ModMatrix::invLinear;
    if (str == "invQuadraticIn") return ModMatrix::invQuadraticIn;
    if (str == "invQuadraticInOut") return ModMatrix::invQuadraticInOut;
    if (str == "invQuadraticOut") return ModMatrix::invQuadraticOut;
    if (str == "invSineIn") return ModMatrix::invSineIn;
    if (str == "invSineInOut") return ModMatrix::invSineInOut;
    if (str == "invSineOut") return ModMatrix::invSineOut;
    if (str == "invExponentialIn") return ModMatrix::invExponentialIn;
    if (str == "invExponentialInOut") return ModMatrix::invExponentialInOut;
    if (str == "invExponentialOut") return ModMatrix::invExponentialOut;
    
    return ModMatrix::linear;
}

juce::String funcToStr (ModMatrix::Function f)
{
    if (f == ModMatrix::linear) return "linear";
    if (f == ModMatrix::quadraticIn) return "quadraticIn";
    if (f == ModMatrix::quadraticInOut) return "quadraticInOut";
    if (f == ModMatrix::quadraticOut) return "quadraticOut";
    if (f == ModMatrix::sineIn) return "sineIn";
    if (f == ModMatrix::sineInOut) return "sineInOut";
    if (f == ModMatrix::sineOut) return "sineOut";
    if (f == ModMatrix::exponentialIn) return "exponentialIn";
    if (f == ModMatrix::exponentialInOut) return "exponentialInOut";
    if (f == ModMatrix::exponentialOut) return "exponentialOut";
    if (f == ModMatrix::invLinear) return "invLinear";
    if (f == ModMatrix::invQuadraticIn) return "invQuadraticIn";
    if (f == ModMatrix::invQuadraticInOut) return "invQuadraticInOut";
    if (f == ModMatrix::invQuadraticOut) return "invQuadraticOut";
    if (f == ModMatrix::invSineIn) return "invSineIn";
    if (f == ModMatrix::invSineInOut) return "invSineInOut";
    if (f == ModMatrix::invSineOut) return "invSineOut";
    if (f == ModMatrix::invExponentialIn) return "invExponentialIn";
    if (f == ModMatrix::invExponentialInOut) return "invExponentialInOut";
    if (f == ModMatrix::invExponentialOut) return "invExponentialOut";
    
    return "linear";
}

//==============================================================================
void ModVoice::startVoice ()
{
    age = owner->voiceStarted (this);
}

void ModVoice::stopVoice()
{
    owner->voiceStopped (this);
}

//==============================================================================
void ModMatrix::stateUpdated (const juce::ValueTree& vt)
{
    auto lookupSrc = [&] (const juce::String& str)
    {
        int idx = 0;
        for (auto& s : sources)
        {
            if (s.id == str)
                return ModSrcId (idx);
            idx++;
        }
        //jassertfalse;
        return ModSrcId();
    };

    for (auto& pi : parameters)
        pi.sources.clear();

    auto mm = vt.getChildWithName ("MODMATRIX");
    if (mm.isValid())
    {
        for (auto c : mm)
        {
            if (! c.hasType ("MODITEM")) continue;

            auto src = c.getProperty ("srcId").toString();
            auto dst = c.getProperty ("dstId").toString();

            auto f = float (c.getProperty ("depth", 0.0f));
            auto e = bool (c.getProperty ("enabled", true));
            auto z = strToFunc (c.getProperty ("function", "linear"));
            auto b = bool (c.getProperty ("biPolarMapping", false));

            if (src.isNotEmpty() && dst.isNotEmpty())
            {
                Source s;
                s.id = lookupSrc (src);
                s.poly = getModSrcPoly (s.id);
                s.depth = f;
                s.enabled = e;
                s.function = z;
                s.biPolarMapping = b;
                
                if (! c.hasProperty ("biPolarMapping"))
                {
                    if (defaultPolarityMode == bipolar)
                        s.biPolarMapping = true;
                    else if (defaultPolarityMode == unipolar)
                        s.biPolarMapping = false;
                    else if (defaultPolarityMode == sameAsSource)
                        s.biPolarMapping = sources[s.id.id].bipolar;
                }

                auto foundParam = false;
                for (auto& pi : parameters)
                {
                    if (pi.parameter->getUid() == dst)
                    {
                        pi.sources.push_back(s);
                        foundParam = true;
                        break;
                    }
                }

                if (! foundParam)
                {
                    DBG("Parameter not found: " + dst);
                    jassertfalse;
                }
            }
        }
    }
    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
}

void ModMatrix::updateState (juce::ValueTree& vt)
{
    auto mm = vt.getOrCreateChildWithName ("MODMATRIX", nullptr);
    mm.removeAllChildren (nullptr);

    for (int i = 0; i < parameters.size(); i++)
    {
        auto& pi = parameters[i];
        for (auto src : pi.sources)
        {
            auto c = juce::ValueTree ("MODITEM");
            c.setProperty ("srcId", sources[src.id.id].id, nullptr);
            c.setProperty ("depth", src.depth, nullptr);
            c.setProperty ("enabled", src.enabled, nullptr);
            c.setProperty ("dstId", pi.parameter->getUid(), nullptr);
            c.setProperty ("function", funcToStr (src.function), nullptr);
            c.setProperty ("biPolarMapping", src.biPolarMapping, nullptr);

            mm.addChild (c, -1, nullptr);
        }
    }
}

void ModMatrix::addVoice (ModVoice* v)
{
    voices.push_back (v);

    v->owner = this;
}

ModSrcId ModMatrix::addMonoModSource (const juce::String& id, const juce::String& name, bool bipolar)
{
    SourceInfo si;
    si.id      = id;
    si.name    = name;
    si.poly    = false;
    si.bipolar = bipolar;
    si.index   = ModSrcId (sources.size());

    sources.push_back (si);
    return ModSrcId (si.index);
}

ModSrcId ModMatrix::addPolyModSource (const juce::String& id, const juce::String& name, bool bipolar)
{
    SourceInfo si;
    si.id      = id;
    si.name    = name;
    si.poly    = true;
    si.bipolar = bipolar;
    si.index   = ModSrcId (sources.size());

    sources.push_back (si);
    return ModSrcId (si.index);
}

void ModMatrix::addParameter (Parameter* p, bool poly, float smoothingTime)
{
    p->setModMatrix (this);
    p->setModIndex (parameters.size());

    ParamInfo pi;
    pi.poly = poly;
    pi.parameter = p;
    pi.smoothingTime = smoothingTime;

    parameters.push_back (pi);
}

void ModMatrix::setSampleRate (double sr)
{
    sampleRate = sr;

    for (auto idx = 0; auto& s : smoothers)
    {
        s[0].setSampleRate (sr);
        s[1].setSampleRate (sr);
        s[0].setTime (parameters[idx].smoothingTime);
        s[1].setTime (parameters[idx].smoothingTime);
        idx++;
    }

    for (auto& v : voices)
    {
        for (auto idx = 0; auto& s : v->smoothers)
        {
            s[0].setSampleRate (sr);
            s[1].setSampleRate (sr);
            s[0].setTime (parameters[idx].smoothingTime);
            s[1].setTime (parameters[idx].smoothingTime);
            idx++;
        }
    }
}

void ModMatrix::build()
{
    smoothers.resize(parameters.size());
    for (int i = 0; i < parameters.size(); ++i)
    {
        auto defaultValue = parameters[i].parameter->getDefaultValue();
        auto& stereoSmoother = smoothers[i];
        stereoSmoother[0].setValue(defaultValue);
        stereoSmoother[0].snapToValue();
        stereoSmoother[1].setValue(defaultValue);
        stereoSmoother[1].snapToValue();
    }

    for (auto& v : voices)
    {
        v->values.resize(sources.size());
        v->smoothers.resize(parameters.size());

        for (int i = 0; i < parameters.size(); ++i)
        {
            auto defaultValue = parameters[i].parameter->getDefaultValue();
            auto& stereoSmoother = v->smoothers[i];  // Access voice smoothers, not main smoothers
            stereoSmoother[0].setValue(defaultValue);
            stereoSmoother[0].snapToValue();
            stereoSmoother[1].setValue(defaultValue);
            stereoSmoother[1].snapToValue();
        }
    }
}

void ModMatrix::enableLearn (ModSrcId src)
{
    learnSource = src;

    listeners.call ([&] (Listener& l) { l.learnSourceChanged (learnSource); });
}

void ModMatrix::disableLearn()
{
    learnSource.id = -1;

    listeners.call ([&] (Listener& l) { l.learnSourceChanged (learnSource); });
}

bool ModMatrix::isModulated (ModDstId param)
{
    auto& pi = parameters[param.id];
    if (pi.sources.size() > 0)
        return true;
    return false;
}

bool ModMatrix::getModEnable (ModSrcId src, ModDstId param)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            return si.enabled;

    return false;
}

void ModMatrix::setModEnable (ModSrcId src, ModDstId param, bool b)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            si.enabled = b;

    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
}

bool ModMatrix::getModBipolarMapping (ModSrcId src, ModDstId param)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            return si.biPolarMapping;

    return false;
}

void ModMatrix::setModBipolarMapping (ModSrcId src, ModDstId param, bool b)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            si.biPolarMapping = b;

    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
}

float ModMatrix::getModDepth (ModSrcId src, ModDstId param)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            return si.depth;

    return 0;
}

ModMatrix::Function ModMatrix::getModFunction (ModSrcId src, ModDstId param)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        if (si.id == src)
            return si.function;

    return Function::linear;
}

std::vector<std::pair<ModSrcId, float>> ModMatrix::getModDepths (ModDstId param)
{
    std::vector<std::pair<ModSrcId, float>> res;

    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
        res.push_back ({si.id, si.depth});

    return res;
}

std::vector<std::pair<ModDstId, float>> ModMatrix::getModDepths (ModSrcId param)
{
    std::vector<std::pair<ModDstId, float>> res;

    for (auto [idx, pi] : juce::enumerate (parameters))
    {
        for (auto& si : pi.sources)
            if (si.id == param)
                res.push_back ({ModDstId (int (idx)), si.depth});
    }

    return res;
}

void ModMatrix::setModDepth (ModSrcId src, ModDstId param, float f)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
    {
        if (si.id == src)
        {
            si.depth = f;

            listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });

            return;
        }
    }

    Source s;
    s.id       = src;
    s.poly     = getModSrcPoly (src);
    s.depth    = f;
    s.function = linear;
    
    if (defaultPolarityMode == bipolar)
        s.biPolarMapping = true;
    else if (defaultPolarityMode == unipolar)
        s.biPolarMapping = false;
    else if (defaultPolarityMode == sameAsSource)
        s.biPolarMapping = sources[src.id].bipolar;

    pi.sources.push_back (s);

    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });

    // Calculate modulation range
    float baseValue = pi.parameter->getValue(); // Current normalized value

    DBG("Modulation connection built:");
    DBG("  Source         : " + sources[src.id].name);
    DBG("  Destination    : " + pi.parameter->getName (1024));
    DBG("  Depth          : " + juce::String (f));
    DBG("  Bipolar        : " + juce::String ((int) s.biPolarMapping));
    DBG("  Base value     : " + juce::String(pi.parameter->getUserRange().convertFrom0to1(baseValue)));
    DBG("  Param Range    : " + juce::String(pi.parameter->getUserRangeStart()) + " to " + juce::String(pi.parameter->getUserRangeEnd()));

}

void ModMatrix::setModFunction (ModSrcId src, ModDstId param, Function f)
{
    auto& pi = parameters[param.id];
    for (auto& si : pi.sources)
    {
        if (si.id == src)
        {
            si.function = f;

            listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });

            return;
        }
    }

    Source s;
    s.id = src;
    s.poly = getModSrcPoly (src);
    s.depth = 1.0f;
    s.function = f;
    
    if (defaultPolarityMode == bipolar)
        s.biPolarMapping = true;
    else if (defaultPolarityMode == unipolar)
        s.biPolarMapping = false;
    else if (defaultPolarityMode == sameAsSource)
        s.biPolarMapping = sources[src.id].bipolar;

    pi.sources.push_back(s);

    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
}

void ModMatrix::clearModDepth (ModSrcId src, ModDstId param)
{
    auto& pi = parameters[param.id];
    for (int i = pi.sources.size(); --i >= 0;)
    {
        auto si = pi.sources[i];
        if (si.id == src)
            pi.sources.erase(pi.sources.begin() + i);
    }

    listeners.call ([&] (Listener& l) { l.modMatrixChanged(); });
}

juce::String ModMatrix::getModDstName (ModDstId param)
{
    auto& pi = parameters[param.id];
    return pi.parameter->getName (1024);
}

juce::Array<ModSrcId> ModMatrix::getModSources (gin::Parameter* param)
{
    juce::Array<ModSrcId> srcs;

    auto idx = param->getModIndex();
    if (idx >= 0)
    {
        auto& pi = parameters[idx];
        for (auto& si : pi.sources)
            srcs.add (si.id);
    }

    return srcs;
}

int ModMatrix::voiceStarted (ModVoice* v)
{
    activeVoice = v;

    return ++nextAge;
}

void ModMatrix::voiceStopped (ModVoice* v)
{
    if (v == activeVoice)
    {
        int youngestVoice = 0;
        activeVoice = nullptr;
        for (auto possibleVoice : voices)
        {
            if (possibleVoice == v) continue;

            if (possibleVoice->isVoiceActive() && possibleVoice->getAge() > youngestVoice)
            {
                youngestVoice = possibleVoice->getAge();
                activeVoice = possibleVoice;
            }
        }
    }
}

Parameter* ModMatrix::getParameter (ModDstId d)
{
    for (const auto& info : parameters)
        if (info.parameter->getModIndex() == d.id)
            return info.parameter;

    return nullptr;
}
