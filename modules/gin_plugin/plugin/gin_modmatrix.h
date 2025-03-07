#pragma once

//==============================================================================
struct ModSrcId
{
    ModSrcId () = default;
    explicit ModSrcId (int id_) : id (id_) {}
    ModSrcId (const ModSrcId& other) { id = other.id; }
    ModSrcId& operator= (const ModSrcId& other) { id = other.id; return *this; }
    bool operator== (const ModSrcId& other) const { return other.id == id; }
    bool isValid() const { return id >= 0; }

    int id = -1;
};

//==============================================================================
struct ModDstId
{
    ModDstId () = default;
    explicit ModDstId (int id_) : id (id_)  {}
    ModDstId (const ModDstId& other) { id = other.id; }
    ModDstId& operator= (const ModDstId& other) { id = other.id; return *this; }
    bool operator== (const ModDstId& other) const { return other.id == id; }
    bool isValid() const { return id >= 0; }

    int id = -1;
};

//==============================================================================
class ModMatrix;

/** Make your synth voice inherit from this if it supports modulation
*/
class ModVoice
{
public:
    ModVoice() = default;
    virtual ~ModVoice() = default;

    /* Gets value of a parameter with modulation applied */
    inline float getValue (gin::Parameter* p);
    inline float getValue(gin::Parameter* p, int channel);
    inline float getValueUnsmoothed (gin::Parameter* p);
    inline float getValueUnsmoothed(gin::Parameter* p, int channel);

    void finishBlock (int numSamples)
    {
        for (auto& s : smoothers)
        {
            s[0].process (numSamples);
            s[1].process (numSamples);
        }

    }

    void snapParams()
    {
        for (auto& s : smoothers)
        {
            s[0].snapToValue();
            s[1].snapToValue();
        }
    }

    void startVoice ();
    void stopVoice();

    int getAge()    { return age; }

    virtual bool isVoiceActive() = 0;

protected:
    bool disableSmoothing = false;

private:
    friend ModMatrix;

    ModMatrix* owner = nullptr;
    std::vector<std::array<float, 2>> values;
    std::vector<std::array<ValueSmoother<float>, 2>> smoothers;
    int age = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModVoice)
};

//==============================================================================
/** Add one of these to you Synth if you want to support modulation

    Then add all your parameters
    Then add all your mod source. Update your mod sources from your processing
    loop.
    Always get your parameter values from the mod matrix
*/
class ModMatrix
{
public:
    ModMatrix() = default;
    
    enum PolarityMode
    {
        unipolar,
        bipolar,
        sameAsSource,
    };
    
    void setDefaultPolarityMode (PolarityMode m) { defaultPolarityMode = m; }

    //==============================================================================
    void stateUpdated (const juce::ValueTree& vt);
    void updateState (juce::ValueTree& vt);
    
    //==============================================================================
    enum Function
    {
        linear,
        
        quadraticIn,
        quadraticInOut,
        quadraticOut,
        
        sineIn,
        sineInOut,
        sineOut,
        
        exponentialIn,
        exponentialInOut,
        exponentialOut,
        
        invLinear,

        invQuadraticIn,
        invQuadraticInOut,
        invQuadraticOut,
        
        invSineIn,
        invSineInOut,
        invSineOut,
        
        invExponentialIn,
        invExponentialInOut,
        invExponentialOut,
    };
    
    static float shape (float v, Function f, bool biPolarSrc, bool biPolarDst)
    {
        if (biPolarSrc)
            v = juce::jmap (v, -1.0f, 1.0f, 0.0f, 1.0f);

        switch (f)
        {
            case invLinear:
            case invQuadraticIn:
            case invQuadraticInOut:
            case invQuadraticOut:
            case invSineIn:
            case invSineInOut:
            case invSineOut:
            case invExponentialIn:
            case invExponentialInOut:
            case invExponentialOut:
                v = 1.0f - v;
                break;
            default:
                break;
        }
        
        switch (f)
        {
            case linear:
            case invLinear:
                break;
            case quadraticIn:
            case invQuadraticIn:
                v = easeQuadraticIn (v);
                break;
            case quadraticInOut:
            case invQuadraticInOut:
                v = easeQuadraticInOut (v);
                break;
            case quadraticOut:
            case invQuadraticOut:
                v = easeQuadraticOut (v);
                break;
            case sineIn:
            case invSineIn:
                v = easeSineIn (v);
                break;
            case sineInOut:
            case invSineInOut:
                v = easeSineInOut (v);
                break;
            case sineOut:
            case invSineOut:
                v = easeSineOut (v);
                break;
            case exponentialIn:
            case invExponentialIn:
                v = easeExponentialIn (v);
                break;
            case exponentialInOut:
            case invExponentialInOut:
                v = easeExponentialInOut (v);
                break;
            case exponentialOut:
            case invExponentialOut:
                v = easeExponentialOut (v);
                break;
        }

        if (biPolarDst)
            v = juce::jmap (v, 0.0f, 1.0f, -1.0f, 1.0f);

        return v;
    }

    float getValue(gin::Parameter* p, int channel, bool smoothed = true)
    {
        const int paramId = p->getModIndex();

        float base = p->getValue();
        auto& info = parameters[paramId];

        for (auto& src : info.sources)
        {
            if (src.enabled)
            {
                if (src.poly && activeVoice != nullptr)
                    base += shape (activeVoice->values[src.id.id][channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                else if (! src.poly)
                    base += shape (sources[src.id.id].monoValues[channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
            }
        }

        base = juce::jlimit (0.0f, 1.0f, base);
        auto& smoother = smoothers[paramId][channel];

        smoother.setValue (base);
        auto v = smoothed ? smoother.getCurrentValue() : base;

        v = p->getUserRange().convertFrom0to1 (v);

        if (p->conversionFunction)
            v = p->conversionFunction (v);

        return v;
    }

    //==============================================================================
    float getValue (gin::Parameter* p, bool smoothed = true)
    {
        jassert(!p->isInternal()); // Internal parameters should not be modulated
        return getValue(p, 0, smoothed);
    }

    float getValue(ModVoice& voice, gin::Parameter* p, int channel, bool smoothed = true)
    {
        const int paramId = p->getModIndex();
        jassert (paramId >= 0);

        float base = p->getValue();
        auto& info = parameters[paramId];

        for (auto& src : info.sources)
        {
            if (src.enabled)
            {
                if (src.poly)
                    base += shape (voice.values[src.id.id][channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                else
                    base += shape (sources[src.id.id].monoValues[channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
            }
        }

        base = juce::jlimit (0.0f, 1.0f, base);
        auto& smoother = voice.smoothers[paramId][channel];

        smoother.setValue (base);
        auto v = (voice.disableSmoothing || ! smoothed) ? base : smoother.getCurrentValue();

        v = p->getUserRange().convertFrom0to1 (v);

        if (p->conversionFunction)
            v = p->conversionFunction (v);

        return v;

    }

    float getValue (ModVoice& voice, gin::Parameter* p, bool smoothed = true)
    {
        return getValue(voice, p, 0, smoothed);
    }

    juce::Array<float> getLiveValues (gin::Parameter* p, int channel)
    {
        juce::Array<float> liveValues;

        const int paramId = p->getModIndex();
        auto& pi = parameters[paramId];

        auto& info = parameters[paramId];

        if (pi.poly)
        {
            for (auto v : voices)
            {
                if (v->isVoiceActive())
                {
                    bool ok = false;
                    float base = p->getValue();

                    for (auto& src : info.sources)
                    {
                        if (src.enabled)
                        {
                            if (src.poly)
                                base += shape (v->values[src.id.id][channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                            else
                                base += shape (sources[src.id.id].monoValues[channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                            ok = true;
                        }
                    }

                    if (ok)
                    {
                        base = juce::jlimit (0.0f, 1.0f, base);
                        liveValues.add (base);
                    }
                }
            }

            if (liveValues.size() == 0)
            {
                float base = p->getValue();
                bool ok = false;

                for (auto& src : info.sources)
                {
                    if (src.enabled && ! src.poly)
                    {
                        base += shape (sources[src.id.id].monoValues[channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                        ok = true;
                    }
                }

                if (ok)
                {
                    base = juce::jlimit (0.0f, 1.0f, base);
                    liveValues.add (base);
                }
            }

        }
        else
        {
            bool ok = false;
            auto v = activeVoice;

            float base = p->getValue();

            for (auto& src : info.sources)
            {
                if (src.enabled)
                {
                    if (src.poly && v != nullptr)
                    {
                        ok = true;
                        base += shape (v->values[src.id.id][channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                    }
                    else if (! src.poly)
                    {
                        ok = true;
                        base += shape (sources[src.id.id].monoValues[channel], src.function, sources[src.id.id].bipolar, src.biPolarMapping) * src.depth;
                    }
                }
            }

            if (ok)
            {
                base = juce::jlimit (0.0f, 1.0f, base);
                liveValues.add (base);
            }
        }

        return liveValues;
    }

    juce::Array<float> getLiveValues (gin::Parameter* p)
    {
        return getLiveValues(p, 0);
    }

    void setMonoValue (ModSrcId id, float value, int channel)
    {
        jassert(channel == 0 || channel == 1);
        auto& info = sources[id.id];
        jassert (! info.poly);

        info.monoValues[channel] = value;
    }

    void setMonoValue (ModSrcId id, float value)
    {
        setMonoValue(id, value, 0);
    }

    void setPolyValue(ModVoice& voice, ModSrcId id, float value, int channel)
    {
        jassert (sources[id.id].poly);
        voice.values[id.id][channel] = value;
    }

    void setPolyValue (ModVoice& voice, ModSrcId id, float value)
    {
        setPolyValue(voice, id, value, 0);
        setPolyValue(voice, id, value, 1);
    }

    void finishBlock (int numSamples)
    {
        for (auto& s : smoothers)
        {
            s[0].process (numSamples);
            s[1].process(numSamples);
        }

    }

    void snapParams()
    {
        for (auto& s : smoothers)
        {
            s[0].snapToValue();
            s[1].snapToValue();
        }
    }

    //==============================================================================
    void addVoice (ModVoice* v);
    ModSrcId addMonoModSource (const juce::String& id, const juce::String& name, bool bipolar);
    ModSrcId addPolyModSource (const juce::String& id, const juce::String& name, bool bipolar);
    void addParameter (gin::Parameter* p, bool poly, float smoothingTime = 0.02f);

    void setSampleRate (double sampleRate);
    void build();

    //==============================================================================
    void enableLearn (ModSrcId source);
    void disableLearn();
    ModSrcId getLearn()                         { return learnSource;               }

    //==============================================================================
    int getNumModSources()                      { return sources.size();            }
    juce::String getModSrcName (ModSrcId src)   { return sources[src.id].name;      }
    bool getModSrcPoly (ModSrcId src)           { return sources[src.id].poly;      }
    bool getModSrcBipolar (ModSrcId src)        { return sources[src.id].bipolar;   }

    juce::String getModDstName (ModDstId dst);

    juce::Array<ModSrcId> getModSources (gin::Parameter*);

    Parameter* getParameter (ModDstId d);

    bool isModulated (ModDstId param);

    float getModDepth (ModSrcId src, ModDstId param);
    std::vector<std::pair<ModDstId, float>> getModDepths (ModSrcId param);
    std::vector<std::pair<ModSrcId, float>> getModDepths (ModDstId param);
    void setModDepth (ModSrcId src, ModDstId param, float f);
    void clearModDepth (ModSrcId src, ModDstId param);
    
    Function getModFunction (ModSrcId src, ModDstId param);
    void setModFunction (ModSrcId src, ModDstId param, Function f);
    
    bool getModEnable (ModSrcId src, ModDstId param);
    void setModEnable (ModSrcId src, ModDstId param, bool b);
    
    bool getModBipolarMapping (ModSrcId src, ModDstId param);
    void setModBipolarMapping (ModSrcId src, ModDstId param, bool b);

    //==============================================================================
    bool shouldShowLiveModValues()
    {
        if (onlyShowModWhenVoiceActive)
        {
            for (auto v : voices)
                if (v->isVoiceActive())
                    return true;

            return false;
        }

        return true;
    }

    void setOnlyShowModWhenVoiceActive (bool b)
    {
        onlyShowModWhenVoiceActive = b;
    }

    //==============================================================================
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void modMatrixChanged()             {}
        virtual void learnSourceChanged (ModSrcId)  {}
    };

    void addListener (Listener* l)      { listeners.add (l);            }
    void removeListener (Listener* l)   { listeners.remove (l);         }

private:
    friend ModVoice;

    //==============================================================================
    int voiceStarted (ModVoice* v);
    void voiceStopped (ModVoice* v);

    //==============================================================================
    struct SourceInfo
    {
        juce::String id;
        juce::String name;
        bool poly = false;
        bool bipolar = false;
        bool stereo = false;
        ModSrcId index = {};
        float monoValues[2] = {0, 0};
    };

    struct Source
    {
        ModSrcId id = {};
        bool poly = false;
        bool enabled = true;
        bool stereo = false;
        float depth = 0.0f;
        bool biPolarMapping = false;
        Function function = ModMatrix::Function::linear;
    };

    struct ParamInfo
    {
        gin::Parameter* parameter = nullptr;
        bool poly = false;
        float smoothingTime = 0.02f;
        std::vector<Source> sources;
    };

    //==============================================================================
    std::vector<SourceInfo> sources;
    std::vector<ParamInfo> parameters;
    std::vector<ModVoice*> voices;
    std::vector<std::array<ValueSmoother<float>, 2>> smoothers;
    // juce::Array<ValueSmoother<float>> smoothers[2];
    ModVoice* activeVoice = nullptr;
    bool onlyShowModWhenVoiceActive = false;
    PolarityMode defaultPolarityMode = sameAsSource;

    double sampleRate = 44100.0;

    juce::ListenerList<Listener> listeners;

    ModSrcId learnSource;
    int nextAge = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModMatrix)
};

inline float ModVoice::getValue (gin::Parameter* p)
{
    jassert(!p->isInternal()); // Internal parameters should not be modulated
    return owner->getValue (*this, p);
}

inline float ModVoice::getValue (gin::Parameter* p, int channel)
{
    return owner->getValue (*this, p, channel);
}

inline float ModVoice::getValueUnsmoothed (gin::Parameter* p)
{
    return owner->getValue (*this, p, false);
}

inline float ModVoice::getValueUnsmoothed(gin::Parameter* p, int channel)
{
    return owner->getValue(*this, p, channel, false);
}