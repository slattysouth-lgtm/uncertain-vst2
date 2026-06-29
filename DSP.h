/*
  ==============================================================================
   DSP.h  —  Room Corrector (Uncertain)  v4  —  VOIX UNIQUEMENT
   Header-only / inline.  Aucune dependance hors JUCE (juce_dsp).

   ══════════════════════════════════════════════════════════════════
   CHAÎNE (signal path)

   AdaptiveGate     —— domaine temporel, driven by CLEAN
   SpectralCleaner  —— UNE SEULE passe FFT (hop=512, latence 1536) :
     Dans un seul passage spectral :
       1. Debruitage        (CLEAN)
       2. Dé-resonance      (CLEAN)
       3. Dé-reverb         (CLEAN)
       4. SmartDeClick      (CLEAN)  — bruits de bouche, claquements
       5. Proximity fix     (REPAIR) — micro trop proche (proximity effect)
       6. U67 curve adapt   (TONE)   — 3 courbes LOW/MID/HIGH, F0 tracking
                                        frame-accurate, réagit aux changements
                                        de note
   Enhancer         —— domaine temporel, driven by TONE
     - Corps  (bas-medium adaptatif)
     - Air    (aigus adaptatifs)
   MixReady         —— domaine temporel, COMP + SIBILANCE séparés
     - De-esser adaptatif   (SIBILANCE)
     - Compresseur + limiter (COMP)
   DelayLine        —— aligne le signal dry avec le signal wet (latence FFT)
   ══════════════════════════════════════════════════════════════════
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <cstring>
#include <array>
#include <vector>

//==============================================================================
//  Helpers
//==============================================================================
static inline float rc_clamp01 (float v) noexcept
{ return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static inline float rc_map (float v, float lo, float hi) noexcept
{ return rc_clamp01 ((v - lo) / (hi - lo + 1.0e-9f)); }

//==============================================================================
//  Biquad  (RBJ, transposee directe II)
//==============================================================================
struct Biquad
{
    double b0=1.0,b1=0.0,b2=0.0,a1=0.0,a2=0.0;
    float  z1=0.0f,z2=0.0f;
    void reset() noexcept { z1=z2=0.0f; }
    inline float process (float x) noexcept
    {
        const float y=(float)(b0*x)+z1;
        z1=(float)(b1*x)-(float)(a1*y)+z2;
        z2=(float)(b2*x)-(float)(a2*y);
        return y;
    }
    void norm (double a0) noexcept { b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0; }
    void setHP (double sr,double f,double Q) noexcept
    {
        const double w=juce::MathConstants<double>::twoPi*f/sr;
        const double c=std::cos(w),s=std::sin(w),a=s/(2.0*Q),a0=1.0+a;
        b0=(1.0+c)*0.5;b1=-(1.0+c);b2=(1.0+c)*0.5;a1=-2.0*c;a2=1.0-a;norm(a0);
    }
    void setLP (double sr,double f,double Q) noexcept
    {
        const double w=juce::MathConstants<double>::twoPi*f/sr;
        const double c=std::cos(w),s=std::sin(w),a=s/(2.0*Q),a0=1.0+a;
        b0=(1.0-c)*0.5;b1=1.0-c;b2=(1.0-c)*0.5;a1=-2.0*c;a2=1.0-a;norm(a0);
    }
    void setBP (double sr,double f,double Q) noexcept
    {
        const double w=juce::MathConstants<double>::twoPi*f/sr;
        const double c=std::cos(w),s=std::sin(w),a=s/(2.0*Q),a0=1.0+a;
        b0=a;b1=0.0;b2=-a;a1=-2.0*c;a2=1.0-a;norm(a0);
    }
};

//==============================================================================
//  EnvFollower
//==============================================================================
struct EnvFollower
{
    float env=0.0f,aA=0.0f,aR=0.0f;
    void setTimes (double sr,float atkMs,float relMs) noexcept
    {
        aA=std::exp(-1.0f/(float)(sr*atkMs*0.001f));
        aR=std::exp(-1.0f/(float)(sr*relMs*0.001f));
    }
    void reset() noexcept { env=0.0f; }
    inline float process (float x) noexcept
    {
        const float r=std::fabs(x);
        env=(r>env)?aA*(env-r)+r:aR*(env-r)+r;
        return env;
    }
};

//==============================================================================
//  AdaptiveGate  —  silence gate  (domaine temporel, AVANT la FFT)
//  Coupe proprement entre les phrases. Plancher de bruit estime dynamiquement.
//  Gain minimum : 0.12 (−18 dB) — jamais silence total → evite les artefacts
//  FFT aux frontieres de frames.
//==============================================================================
class AdaptiveGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr=sampleRate;
        aRms=std::exp(-1.0/(sr*0.040));
        aAtk=std::exp(-1.0/(sr*0.005));
        aRel=std::exp(-1.0/(sr*0.120));
        reset();
    }
    void reset() noexcept { rmsVal=noiseFloor=1.0e-4f; gGain=1.0f; }

    inline float process (float x, float amount) noexcept
    {
        if (amount<=0.0001f) return x;

        rmsVal=(float)aRms*rmsVal+(1.0f-(float)aRms)*std::fabs(x);

        // Estimation du plancher de bruit
        if (rmsVal<noiseFloor) noiseFloor=0.55f*noiseFloor+0.45f*rmsVal;
        else                   noiseFloor=0.9997f*noiseFloor+0.0003f*rmsVal;
        noiseFloor=juce::jmax(1.0e-6f,noiseFloor);

        // Seuil : amount grand = seuil plus haut = gate plus agressif
        const float thr    = noiseFloor*(4.0f+8.0f*amount);
        const float target = (rmsVal>=thr) ? 1.0f
                           : juce::jlimit(0.12f,1.0f,rmsVal/thr);
        const float a=(target>gGain)?(float)aAtk:(float)aRel;
        gGain=a*gGain+(1.0f-a)*target;

        return x*(1.0f-amount+amount*gGain);
    }

private:
    double sr=44100.0,aRms=0.99,aAtk=0.999,aRel=0.9997;
    float  rmsVal=1.0e-4f,noiseFloor=1.0e-4f,gGain=1.0f;
};

//==============================================================================
//  SpectralCleaner  —  UNE SEULE passe FFT (overlap-add 75 %)
//
//  Paramètres d'entrée :
//    cleanAmt  (CLEAN 0..1)  → denoise + de-resonance + dereverb + de-click
//    repairAmt (REPAIR 0..1) → correction de proximite (mic trop proche)
//    toneAmt   (TONE 0..1)   → match de courbe U67 (adaptatif F0, 3 profils)
//
//  Traitements (par bin, dans l'ordre, gain multiplicatif) :
//    gDenoise  — soustraction spectrale (plancher de bruit)
//    gDeRes    — dé-resonance chirurgicale (coupe les pics > enveloppe locale)
//    gDeRev    — dé-reverb (soustraction de la traine etalee)
//    gDeClick  — SmartDeClick (bruits de bouche : fronts transitoires broadband)
//    gProx     — Proximity Fix (attenue les basses quand trop proche du micro)
//    gTone     — Courbe cible U67 blendee selon F0 detecte
//
//  gTotal = gDenoise * gDeRes * gDeRev * gDeClick * gProx * gTone
//==============================================================================
class SpectralCleaner
{
public:
    static constexpr int order   = 11;           // 2^11 = 2048
    static constexpr int size    = 1<<order;     // 2048
    static constexpr int osamp   = 4;            // overlap 75 %
    static constexpr int hop     = size/osamp;   // 512
    static constexpr int latency = size-hop;     // 1536
    static constexpr int bins    = size/2+1;     // 1025

    void prepare (double sampleRate)
    {
        sr=sampleRate;
        for (int n=0;n<size;++n)
        {
            const float h=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*(float)n/(float)size);
            window[n]=std::sqrt(h);
        }
        synthScale=2.0f/(float)osamp;

        buildCurve(tgtLow,  2300.f,0.5f,-1.5f); // voix grave, baritone
        buildCurve(tgtMid,  3000.f,2.0f,-4.5f); // tenor/mezzo, standard U67
        buildCurve(tgtHigh, 3700.f,2.8f,-7.5f); // soprano, falsetto

        const float bHz=(float)sr/(float)size;
        loN    =juce::jlimit(1,bins-2,(int)(300.f /bHz));
        hiN    =juce::jlimit(1,bins-1,(int)(3000.f/bHz));
        f0Lo   =juce::jlimit(1,bins-1,(int)(80.f  /bHz));
        f0Hi   =juce::jlimit(1,bins-1,(int)(500.f /bHz));
        lo200  =juce::jlimit(1,bins-1,(int)(200.f /bHz));
        hi2k   =juce::jlimit(1,bins-1,(int)(2000.f/bHz));
        loClick=juce::jlimit(1,bins-1,(int)(300.f /bHz));
        hiClick=juce::jlimit(1,bins-1,(int)(5000.f/bHz));

        computeRef(tgtLow, refLow);
        computeRef(tgtMid, refMid);
        computeRef(tgtHigh,refHigh);

        reset();
    }

    void reset()
    {
        inFIFO.fill(0.0f); outFIFO.fill(0.0f); outputAccum.fill(0.0f);
        noiseFloor.fill(1.0e-6f); prevGain.fill(1.0f);
        avgMag.fill(0.0f); prevMatch.fill(1.0f);
        tail.fill(0.0f); magPrev.fill(0.0f);
        runPeak=0.0f; gRover=latency;
        f0Smooth=200.0f; vBlend=0.5f;
        clickSmooth=0.0f; proxSmooth=0.35f;
    }

    // Appel echantillon par echantillon (appelé dans le processBlock principal)
    inline float processSample (float x, float cleanAmt, float repairAmt, float toneAmt) noexcept
    {
        inFIFO[gRover]=x;
        const float out=outFIFO[gRover-latency];
        if (++gRover>=size) { gRover=latency; processFrame(cleanAmt,repairAmt,toneAmt); }
        return out;
    }

private:
    //  Courbe cible "Neumann U67 en piece traitee" parametrique.
    //  presenceHz : pic de presence
    //  airGainDb  : boost au-dessus de 10 kHz
    //  lowRollDb  : roll-off des graves sous 80 Hz (negatif)
    void buildCurve (std::array<float,bins>& tgt,
                     float presenceHz, float airGainDb, float lowRollDb) noexcept
    {
        static constexpr int NP=14;
        const float bHz=(float)sr/(float)size;

        // Points de controle : [freq, dB]
        struct Pt { float f, db; };
        Pt pts[NP]={
            {30.f,  -9.0f+lowRollDb},
            {80.f,  -3.0f+lowRollDb*0.4f},
            {150.f,  0.0f},
            {300.f, -1.5f},
            {600.f, -1.0f},
            {1000.f, 0.0f},
            {2000.f, 0.5f},
            {presenceHz, 1.8f},          // pic de presence adaptatif
            {4000.f, 1.0f+airGainDb*0.4f},
            {6000.f, 1.5f+airGainDb*0.6f},
            {8000.f, 1.8f+airGainDb},
            {11000.f,2.0f+airGainDb},
            {14000.f,1.8f+airGainDb*0.8f},
            {18000.f,1.4f+airGainDb*0.5f}
        };

        for (int k=0;k<bins;++k)
        {
            const float f=juce::jmax(bHz*0.5f,(float)k*bHz);
            float gdb;
            if      (f<=pts[0].f)      gdb=pts[0].db;
            else if (f>=pts[NP-1].f)   gdb=pts[NP-1].db;
            else
            {
                int i=0; while(i<NP-1&&f>pts[i+1].f) ++i;
                const float t=(std::log(f)-std::log(pts[i].f))
                             /(std::log(pts[i+1].f)-std::log(pts[i].f));
                gdb=pts[i].db+t*(pts[i+1].db-pts[i].db);
            }
            tgt[k]=std::pow(10.0f,gdb/20.0f);
        }
    }

    void computeRef (const std::array<float,bins>& tgt, float& ref) noexcept
    {
        float s=0.0f;
        for (int k=loN;k<=hiN;++k) s+=tgt[k];
        ref=s/(float)(hiN-loN+1)+1.0e-9f;
    }

    void processFrame (float cleanAmt, float repairAmt, float toneAmt) noexcept
    {
        // ── Fenêtrage + FFT ──────────────────────────────────────────────
        for (int k=0;k<size;++k)       fftWork[k]=inFIFO[k]*window[k];
        for (int k=size;k<2*size;++k)  fftWork[k]=0.0f;
        fft.performRealOnlyForwardTransform(fftWork.data());

        for (int k=0;k<bins;++k)
        {
            const float re=fftWork[2*k],im=fftWork[2*k+1];
            mag[k]=std::sqrt(re*re+im*im);
        }

        // ── Plancher de bruit (suit le minimum, remonte lentement) ───────
        for (int k=0;k<bins;++k)
        {
            if (mag[k]<noiseFloor[k]) noiseFloor[k]+=0.50f  *(mag[k]-noiseFloor[k]);
            else                      noiseFloor[k]+=0.0008f*(mag[k]-noiseFloor[k]);
        }

        // ── Enveloppe spectrale lissee (de-resonance) ────────────────────
        float acc=mag[0];
        for (int k=0;k<bins;++k)    { acc+=0.30f*(mag[k]-acc); env[k]=acc; }
        acc=env[bins-1];
        for (int k=bins-1;k>=0;--k) { acc+=0.30f*(env[k]-acc); env[k]=0.5f*(env[k]+acc); }

        // ── Moyenne long-terme + somme prefixe ───────────────────────────
        float frameRMS=0.0f;
        for (int k=0;k<bins;++k) frameRMS+=mag[k]*mag[k];
        frameRMS=std::sqrt(frameRMS/(float)bins);
        if (frameRMS>runPeak) runPeak=frameRMS;
        else                  runPeak+=0.001f*(frameRMS-runPeak);

        const bool  active=(frameRMS>1.0e-4f)&&(frameRMS>0.2f*runPeak);
        const float aAvg  =active?0.02f:0.0f;
        for (int k=0;k<bins;++k) avgMag[k]+=aAvg*(mag[k]-avgMag[k]);

        prefix[0]=0.0f;
        for (int k=0;k<bins;++k) prefix[k+1]=prefix[k]+avgMag[k];

        float mRef=0.0f;
        for (int k=loN;k<=hiN;++k)
        {
            const int lo=juce::jmax(0,      (int)((float)k*0.707f));
            const int hi=juce::jmin(bins-1, (int)((float)k*1.414f)+1);
            mRef+=(prefix[hi+1]-prefix[lo])/(float)(hi-lo+1);
        }
        mRef=mRef/(float)(hiN-loN+1)+1.0e-9f;

        // ── Detection F0 (frame-accurate, reagit aux changements de note) ─
        {
            int   peakBin=f0Lo; float peakMag=0.0f;
            for (int k=f0Lo;k<=f0Hi;++k)
                if (mag[k]>peakMag) { peakMag=mag[k]; peakBin=k; }

            // Met a jour seulement si signal vocal present (pas sur le silence)
            if (peakMag*peakMag>0.03f*frameRMS*frameRMS+1.0e-10f)
            {
                const float f0Est=(float)peakBin*(float)sr/(float)size;
                // 0.15f = ~7 frames de constante de temps → reagit rapidement
                f0Smooth+=0.15f*(f0Est-f0Smooth);
            }
            // blend : 0.0=LOW(<120Hz) … 0.5=MID(~200Hz) … 1.0=HIGH(>320Hz)
            const float tVB=rc_map(f0Smooth,120.0f,320.0f);
            vBlend+=0.12f*(tVB-vBlend); // suit les variations de note
        }

        // ── Detection SmartDeClick ────────────────────────────────────────
        // Bruits de bouche, claquements, pops : transitoires broadband courts.
        // Signature : elevation simultanee de nombreux bins dans 300Hz-5kHz.
        {
            int clickBins=0;
            for (int k=loClick;k<=hiClick;++k)
                if (avgMag[k]>1.0e-5f && mag[k]>avgMag[k]*3.5f) ++clickBins;
            const float cf=(float)clickBins/(float)(hiClick-loClick+1);
            // Lissage : attaque rapide, relachement rapide (l'evenement est court)
            clickSmooth=0.35f*clickSmooth+0.65f*(cf>0.42f?1.0f:0.0f);
        }

        // ── Detection Proximity ───────────────────────────────────────────
        // Ratio energie sub-bass (<200Hz) / mid (200-2kHz).
        // Eleve = micro trop proche → proximity effect (bosse de basses).
        {
            float subE=0.0f,midE=0.0f;
            for (int k=1;k<lo200;++k)   subE+=avgMag[k];
            for (int k=lo200;k<hi2k;++k) midE+=avgMag[k];
            const float pr=subE/(midE+1.0e-9f);
            proxSmooth+=0.008f*(pr-proxSmooth); // tres lent (caracteristique du setup)
        }

        const bool doMatch=(toneAmt>0.0001f)&&(mRef>1.0e-6f);
        const bool doRev  =(cleanAmt>0.0001f);
        const bool doClick=(cleanAmt>0.001f)&&(clickSmooth>0.15f);
        const bool doProx =(repairAmt>0.001f)&&(proxSmooth>0.38f); // seuil ~neutre

        const float overSub  =1.0f+cleanAmt*3.0f;
        const float gdFloor  =1.0f-0.95f*cleanAmt;
        const float resThr   =1.0f+(1.0f-cleanAmt)*1.1f+0.20f;
        const float resFloor =0.40f;

        for (int k=0;k<bins;++k)
        {
            // 1) DENOISE — soustraction spectrale
            const float ne=noiseFloor[k]*overSub;
            float gd=(mag[k]-ne)/(mag[k]+1.0e-9f);
            gd=gdFloor+(1.0f-gdFloor)*rc_clamp01(gd);

            // 2) DÉ-RESONANCE — coupe chirurgicale des pics (+env locale)
            const float excess=mag[k]/(env[k]+1.0e-9f);
            float gr=1.0f;
            if (excess>resThr)
                gr=juce::jmax(resFloor,std::pow(resThr/excess,1.1f*cleanAmt));

            const float gClean=rc_clamp01(gd*gr);
            prevGain[k]+=0.5f*(gClean-prevGain[k]);
            const float gCleanApplied=1.0f-cleanAmt*(1.0f-prevGain[k]);

            // 3) DÉ-REVERB — attenue les bins domines par la traine tardive
            float gRev=1.0f;
            if (doRev)
            {
                const float ratio=tail[k]/(mag[k]+1.0e-9f);
                gRev=1.0f-cleanAmt*0.85f*rc_clamp01(ratio);
                gRev=juce::jlimit(0.25f,1.0f,gRev);
            }
            tail[k]=0.90f*tail[k]+0.10f*magPrev[k];
            magPrev[k]=mag[k];

            // 4) SMART DE-CLICK — remplace le spectre du click par le modele lisse
            float gClick=1.0f;
            if (doClick)
            {
                const float clickStr=cleanAmt*juce::jmin(1.0f,clickSmooth*1.6f);
                const float toModel =avgMag[k]/(mag[k]+1.0e-9f);
                gClick=1.0f-clickStr*0.88f*(1.0f-juce::jmin(1.0f,toModel));
                gClick=juce::jmax(0.12f,gClick);
            }

            // 5) PROXIMITY FIX — attenue les basses si micro trop proche
            float gProx=1.0f;
            if (doProx && k<lo200)
            {
                // proxSmooth > 0.38 = trop de bass → on attenue DOUCEMENT
                const float excess2=proxSmooth/0.38f;
                gProx=1.0f/(1.0f+repairAmt*(excess2-1.0f)*0.45f);
                gProx=juce::jmax(0.45f,gProx); // max -7 dB (doux, ne detruit pas)
            }

            // 6) COURBE U67 ADAPTATIVE (3 profils blendees selon F0)
            float gTone=1.0f;
            if (doMatch)
            {
                const int lo=juce::jmax(0,      (int)((float)k*0.707f));
                const int hi=juce::jmin(bins-1, (int)((float)k*1.414f)+1);
                const float smooth=(prefix[hi+1]-prefix[lo])/(float)(hi-lo+1)+1.0e-12f;

                float tgtV, refV;
                if (vBlend<=0.5f)
                {
                    const float t=vBlend*2.0f;
                    tgtV=tgtLow[k]*(1.0f-t)+tgtMid[k]*t;
                    refV=refLow  *(1.0f-t)+refMid  *t;
                }
                else
                {
                    const float t=(vBlend-0.5f)*2.0f;
                    tgtV=tgtMid[k]*(1.0f-t)+tgtHigh[k]*t;
                    refV=refMid  *(1.0f-t)+refHigh  *t;
                }
                refV=juce::jmax(1.0e-9f,refV);

                float ratio=(tgtV/refV)/(smooth/mRef);
                ratio=juce::jlimit(0.316f,3.162f,ratio);     // +/-10 dB
                gTone=std::pow(ratio,toneAmt);
            }
            prevMatch[k]+=0.25f*(gTone-prevMatch[k]);

            // Gain total : produit des 6 contributions
            const float gTotal=gCleanApplied*gRev*gClick*gProx*prevMatch[k];

            fftWork[2*k]   *=gTotal;
            fftWork[2*k+1] *=gTotal;
            if (k>0&&k<size/2)
            {
                const int m=size-k;
                fftWork[2*m]   *=gTotal;
                fftWork[2*m+1] *=gTotal;
            }
        }

        fft.performRealOnlyInverseTransform(fftWork.data());

        for (int k=0;k<size;++k)
            outputAccum[k]+=window[k]*fftWork[k]*synthScale;
        for (int k=0;k<hop;++k) outFIFO[k]=outputAccum[k];
        std::memmove(outputAccum.data(),outputAccum.data()+hop,(size_t)size*sizeof(float));
        for (int k=size;k<size+hop;++k) outputAccum[k]=0.0f;
        for (int k=0;k<latency;++k) inFIFO[k]=inFIFO[k+hop];
    }

    double sr=44100.0;
    float  synthScale=0.5f,runPeak=0.0f;
    float  f0Smooth=200.0f,vBlend=0.5f;
    float  clickSmooth=0.0f,proxSmooth=0.35f;
    int    gRover=latency;
    int    loN=14,hiN=139,f0Lo=4,f0Hi=23;
    int    lo200=5,hi2k=47,loClick=7,hiClick=116;
    float  refLow=1.0f,refMid=1.0f,refHigh=1.0f;

    juce::dsp::FFT fft{order};

    std::array<float,size>    window{},inFIFO{},outFIFO{};
    std::array<float,2*size>  outputAccum{},fftWork{};
    std::array<float,bins>    mag{},env{},noiseFloor{},prevGain{};
    std::array<float,bins>    avgMag{},prevMatch{};
    std::array<float,bins>    tail{},magPrev{};
    std::array<float,bins>    tgtLow{},tgtMid{},tgtHigh{};
    std::array<float,bins+1>  prefix{};
};

//==============================================================================
//  Enhancer  —  CORPS (bas-medium) + AIR (aigus), adaptatif, driven by TONE
//==============================================================================
class Enhancer
{
public:
    void prepare (double sr)
    {
        lp320.setLP(sr,320.0,0.707);
        hp3k .setHP(sr,3000.0,0.707);
        hp8k .setHP(sr,8000.0,0.707);
        hfEnv.setTimes(sr,5.0f,80.0f);
        fEnv .setTimes(sr,5.0f,80.0f);
        lEnv .setTimes(sr,5.0f,120.0f);
        reset();
    }
    void reset() { lp320.reset();hp3k.reset();hp8k.reset();hfEnv.reset();fEnv.reset();lEnv.reset(); }

    inline float processSample (float x, float amount) noexcept
    {
        if (amount<=0.0001f) return x;

        // ── Detection par bandes ─────────────────────────────────────────
        const float low =lp320.process(x);
        const float hp3 =hp3k.process(x);
        const float hp8 =hp8k.process(x);

        const float ffe =fEnv .process(x)  +1.0e-6f; // energie large bande
        const float hfe =hfEnv.process(hp8)+1.0e-9f; // energie aigus
        const float le  =lEnv .process(low)+1.0e-6f; // energie grave

        // manque d'aigus (dull) / manque de grave (thin), 0..1
        const float dull=1.0f-rc_clamp01((hfe/ffe)*3.0f);
        const float thin=1.0f-rc_clamp01((le /ffe)*1.5f);

        // GARDE ANTI-SIBILANCE : si les aigus sont DEJA forts → on bride l'air.
        // (1 = peu d'aigus, on peut booster ; 0 = deja sifflant, on ne touche pas)
        const float sibGuard=1.0f-rc_clamp01((hfe/ffe)*6.0f);

        // ── CORPS : comble le grave manquant, doux, jamais boueux ─────────
        const float body=(std::tanh(low*(1.2f+amount*1.8f))-low)
                        *amount*(0.15f+0.40f*thin);

        // ── AIR : boost lineaire PROPRE (pas de tanh => pas de fizz),
        //    dose par le manque d'aigus, et bride par la garde anti-sibilance ─
        const float presence=hp3*amount*0.22f*(0.5f+0.5f*dull);
        const float air     =hp8*amount*(0.30f+0.80f*dull)*sibGuard;

        // ── FILL : saturation TRES legere, uniquement si le signal est pauvre
        //    (record faible) et a tres bas niveau → comble les vides sans colorer ─
        const float lack     =rc_clamp01((dull+thin)*0.5f);
        const float fillDrive=1.0f+amount*lack*1.5f;
        const float filled   =std::tanh(x*fillDrive)/fillDrive;
        const float fill     =(filled-x)*amount*lack*0.25f;

        return x+body+presence+air+fill;
    }

private:
    Biquad      lp320,hp3k,hp8k;
    EnvFollower hfEnv,fEnv,lEnv;
};

//==============================================================================
//  MixReady  —  De-esser (SIBILANCE) + Compresseur+Limiteur (COMP)
//  Deux parametres independants, detection stereo liee.
//==============================================================================
class MixReady
{
public:
    void prepare (double sampleRate, int)
    {
        sr=sampleRate;
        for (auto& b:sibHP) b.setHP(sr,6200.0,0.707);
        sibEnv.setTimes(sr,1.0f,55.0f);
        fastEnv.setTimes(sr,1.0f,60.0f);    // detecteur rapide : capte plosives/pics
        slowEnv.setTimes(sr,35.0f,320.0f);  // detecteur lent : suit le corps de la voix
        lvlTrack.setTimes(sr,250.0f,700.0f);
        reset();
    }
    void reset()
    {
        for (auto& b:sibHP) b.reset();
        sibEnv.reset();fastEnv.reset();slowEnv.reset();lvlTrack.reset();
        compGain=1.0f;limGain=1.0f;makeupSm=1.0f;holdCtr=0;
        compSm=0.0f;sibSm=0.0f;
    }

    void process (juce::AudioBuffer<float>& buf, float compAmt, float sibAmt) noexcept
    {
        const int chs=juce::jmin(2,buf.getNumChannels());
        const int n  =buf.getNumSamples();
        const float ceil_=0.89f; // −1 dBFS

        for (int s=0;s<n;++s)
        {
            compSm+=0.002f*(compAmt-compSm);
            sibSm +=0.002f*(sibAmt -sibSm);

            float mono=0.0f;
            for (int c=0;c<chs;++c) mono=juce::jmax(mono,std::fabs(buf.getSample(c,s)));
            const float lvl=lvlTrack.process(mono)+1.0e-6f;

            // ── De-esser (SIBILANCE) ─────────────────────────────────────
            if (sibSm>0.0005f)
            {
                float hp[2]={},det=0.0f;
                for (int c=0;c<chs;++c)
                { hp[c]=sibHP[c].process(buf.getSample(c,s)); det=juce::jmax(det,std::fabs(hp[c])); }
                const float se  =sibEnv.process(det);
                const float sThr=lvl*(0.65f-0.45f*sibSm)+1.0e-6f;
                float deessGR=1.0f;
                if (se>sThr)
                    deessGR=juce::jlimit(0.08f,1.0f,std::pow(1.0f/(se/sThr),0.75f*sibSm));
                for (int c=0;c<chs;++c)
                { const float x=buf.getSample(c,s); buf.setSample(c,s,(x-hp[c])+hp[c]*deessGR); }
            }

            // ── Compresseur ANTI-POMPAGE (COMP) ──────────────────────────
            //  - knee doux, ratio doux
            //  - 2 detecteurs : lent (corps) + rapide (transitoires/plosives)
            //  - HOLD + release long program-dependent => zero pompage
            //  - makeup lisse tres lentement => pas de respiration de volume
            //  - aucune coloration ajoutee (signal propre)
            if (compSm>0.0005f)
            {
                float m2=0.0f;
                for (int c=0;c<chs;++c) m2=juce::jmax(m2,std::fabs(buf.getSample(c,s)));

                const float fast=fastEnv.process(m2)+1.0e-9f;
                const float slow=slowEnv.process(m2)+1.0e-9f;
                const float det =juce::jmax(slow,fast*0.85f); // corps, mais attrape les pics

                const float thr  =juce::jmax(0.04f,0.10f-0.05f*compSm);
                const float ratio=1.0f+compSm*3.0f;   // jusqu'a ~4:1, doux
                const float knee =6.0f;               // dB, knee doux

                // gain computer soft-knee (en dB)
                const float xDb  =20.0f*std::log10(det);
                const float thrDb=20.0f*std::log10(thr);
                const float over =xDb-thrDb;
                float grDb=0.0f;
                if (2.0f*over>knee)        grDb=(1.0f/ratio-1.0f)*over;
                else if (2.0f*over>-knee)  { const float z=over+knee*0.5f;
                                             grDb=(1.0f/ratio-1.0f)*(z*z)/(2.0f*knee); }
                const float target=std::pow(10.0f,grDb/20.0f); // <=1

                // lissage anti-pompage : attaque rapide, HOLD, puis release long
                const float atkA=std::exp(-1.0f/(float)(sr*0.005f));        // 5 ms
                const float relMs=160.0f+260.0f*compSm;                     // 160→420 ms
                const float relA=std::exp(-1.0f/(float)(sr*relMs*0.001f));
                if (target<compGain)            // le gain descend (attaque)
                { compGain=atkA*(compGain-target)+target; holdCtr=(int)(sr*0.025f); }
                else if (holdCtr>0)             // maintien : on ne relache pas encore
                    --holdCtr;
                else                            // release lent => pas de pompage
                    compGain=relA*(compGain-target)+target;

                // makeup auto, doux et TRES lisse (jamais de respiration)
                const float makeup=std::pow(juce::jlimit(0.5f,4.0f,0.20f/slow),0.6f*compSm);
                makeupSm+=0.0006f*(makeup-makeupSm);

                const float gtot=compGain*makeupSm;
                for (int c=0;c<chs;++c)
                    buf.setSample(c,s,buf.getSample(c,s)*gtot);
            }

            // ── Limiteur de securite (COMP > 0) ──────────────────────────
            if (compSm>0.001f)
            {
                float peak=0.0f;
                for (int c=0;c<chs;++c) peak=juce::jmax(peak,std::fabs(buf.getSample(c,s)));
                if (peak>ceil_) { const float g=ceil_/peak; if (g<limGain) limGain=g; }
                limGain+=(1.0f-limGain)*0.0008f;
                for (int c=0;c<chs;++c)
                    buf.setSample(c,s,juce::jlimit(-ceil_,ceil_,buf.getSample(c,s)*limGain));
            }
        }
    }

private:
    double sr=44100.0;
    Biquad sibHP[2];
    EnvFollower sibEnv,fastEnv,slowEnv,lvlTrack;
    float compGain=1.0f,limGain=1.0f,makeupSm=1.0f,compSm=0.0f,sibSm=0.0f;
    int   holdCtr=0;
};

//==============================================================================
//  DelayLine  —  retard du signal dry pour aligner le blend dry/wet (latence FFT)
//==============================================================================
class DelayLine
{
public:
    void prepare (int maxDelay, int maxBlock)
    {
        sizeBuf=maxDelay+juce::jmax(1,maxBlock)+4;
        for (auto& b:buf) b.assign((size_t)sizeBuf,0.0f);
        delay=maxDelay; widx=0;
    }
    void setDelay (int d) noexcept { delay=d; }
    void reset() { for (auto& b:buf) std::fill(b.begin(),b.end(),0.0f); widx=0; }

    inline float read (int ch, float x) noexcept
    {
        buf[ch][(size_t)widx]=x;
        int r=widx-delay; if(r<0) r+=sizeBuf;
        return buf[ch][(size_t)r];
    }
    inline void advance() noexcept { if(++widx>=sizeBuf) widx=0; }

private:
    std::vector<float> buf[2];
    int sizeBuf=1,delay=0,widx=0;
};
