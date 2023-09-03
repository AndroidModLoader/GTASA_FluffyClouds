#include <mod/amlmod.h>
#include <mod/logger.h>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
    #define BYVER(__for32, __for64) (__for32)
#else
    #include "GTASA_STRUCTS_210.h"
    #define BYVER(__for32, __for64) (__for64)
#endif
#define sizeofA(__aVar)  ((int)(sizeof(__aVar)/sizeof(__aVar[0])))

MYMOD(net.theartemmaps.rusjj.fluff, FluffyCloudsSA, 1.0, TheArtemMaps & RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0.2.2)
END_DEPLIST()

uintptr_t pGTASA;
void* hGTASA;

#define MAX_FLUFFY_CLOUDS 37
#define FLUFF_Z_OFFSET 50.0f // 40.0f
#define FLUFF_ALPHA 180

#define NO_FLUFF_AT_HEIGHTS

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Game Variables
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
float *Foggyness, *ExtraSunnyness, *ms_cameraRoll, *CloudRotation, *SunScreenX, *SunScreenY, *CloudCoverage;
unsigned int *IndividualRotation;
CCamera *TheCamera;
bool *SunBlockedByClouds;
CColourSet *m_CurrentColours;
RsGlobalType *RsGlobal;
TextureDatabaseRuntime* FluffyCloudsTexDB;

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Game Functions
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
void (*RwRenderStateSet)(RwRenderState, void*);
float (*GetATanOfXY)(float, float);
bool (*CalcScreenCoors)(RwV3d const&, RwV3d*, float *, float *, bool, bool);
void (*RenderBufferedOneXLUSprite_Rotate_2Colours)(float,float,float,float,float,unsigned char,unsigned char,unsigned char,unsigned char,unsigned char,unsigned char,float,float,float,float,unsigned char);
void (*RenderBufferedOneXLUSprite_Rotate_Aspect)(float,float,float,float,float,unsigned char,unsigned char,unsigned char,short,float,float,unsigned char);
void (*FlushSpriteBuffer)();
TextureDatabaseRuntime* (*TextureDatabaseLoad)(const char*, bool, TextureDatabaseFormat);
int (*GetEntry)(TextureDatabaseRuntime *,char const*, bool*);
RwTexture* (*GetRWTexture)(TextureDatabaseRuntime *, int);
bool (*CanSeeOutSideFromCurrArea)();
void (*InitSpriteBuffer)();

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Variables
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
RwTexture* gpCloudTex[5];
bool CloudOnScreen[MAX_FLUFFY_CLOUDS];

float CoorsOffsetX[MAX_FLUFFY_CLOUDS] = {
    0.0f, 60.0f, 72.0f, 48.0f, 21.0f, 12.0f,
    9.0f, -3.0f, -8.4f, -18.0f, -15.0f, -36.0f,
    -40.0f, -48.0f, -60.0f, -24.0f, 100.0f, 100.0f,
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
    100.0f, 100.0f, -30.0f, -20.0f, 10.0f, 30.0f,
    0.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f
};
float CoorsOffsetY[MAX_FLUFFY_CLOUDS] = {
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
    100.0f, 100.0f, 100.0f, 100.0f, -30.0f, 10.0f,
    -25.0f, -5.0f, 28.0f, -10.0f, 10.0f, 0.0f,
    15.0f, 40.0f, -100.0f, -100.0f, -100.0f, -100.0f,
    -100.0f, -40.0f, -20.0f, 0.0f, 10.0f, 30.0f, 35.0f
};
float CoorsOffsetZ[MAX_FLUFFY_CLOUDS] = {
    2.0f, 1.0f, 0.0f, 0.3f, 0.7f, 1.4f,
    1.7f, 0.24f, 0.7f, 1.3f, 1.6f, 1.0f,
    1.2f, 0.3f, 0.7f, 1.4f, 0.0f, 0.1f,
    0.5f, 0.4f, 0.55f, 0.75f, 1.0f, 1.4f,
    1.7f, 2.0f, 2.0f, 2.3f, 1.9f, 2.4f,
    2.0f, 2.0f, 1.5f, 1.2f, 1.7f, 1.5f, 2.1f
};

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Functions
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
inline float SQR(float v) { return v*v; }
inline RwTexture* GetTextureFromTexDB(TextureDatabaseRuntime* texdb, const char* name)
{
    bool hasSiblings;
    return GetRWTexture(texdb, GetEntry(texdb, name, &hasSiblings));
}
inline void RenderFluffyClouds()
{
    *SunBlockedByClouds = false;
    int fluffyalpha = FLUFF_ALPHA * (1.0f - fmax(*Foggyness, *ExtraSunnyness));

    CVector campos = TheCamera->GetPosition();
#ifdef NO_FLUFF_AT_HEIGHTS
    if(campos.z > FLUFF_Z_OFFSET)
    {
        fluffyalpha -= (campos.z - FLUFF_Z_OFFSET) * ((float)(255 - (unsigned char)FLUFF_ALPHA) / 255.0f);
    }
#endif

    if(fluffyalpha > 0)
    {
        InitSpriteBuffer();

        float sundist, hilight;
        float szx, szy;
        RwV3d screenpos, worldpos;
        int distLimit = (3 * RsGlobal->maximumWidth) / 4;
        int sundistBlocked = RsGlobal->maximumWidth / 10;
        int sundistHilit = RsGlobal->maximumWidth / 3;

        float rot_sin = sinf(*CloudRotation);
        float rot_cos = cosf(*CloudRotation);

        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)0);
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)0);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);

        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, gpCloudTex[3]->raster);
        for(int i = 0; i < MAX_FLUFFY_CLOUDS; ++i)
        {
            RwV3d pos = { 2.0f * CoorsOffsetX[i], 2.0f * CoorsOffsetY[i], 40.0f * CoorsOffsetZ[i] + FLUFF_Z_OFFSET };
            worldpos.x = pos.x * rot_cos + pos.y * rot_sin + campos.x;
            worldpos.y = pos.x * rot_sin - pos.y * rot_cos + campos.y;
            worldpos.z = pos.z;

            if (CalcScreenCoors(worldpos, &screenpos, &szx, &szy, false, false))
            {
                sundist = sqrtf(SQR(screenpos.x - *SunScreenX) + SQR(screenpos.y - *SunScreenY));
                int tr = 0;
                int tg = 0;
                int tb = 0;
                int br = m_CurrentColours->fluffycloudr;
                int bg = m_CurrentColours->fluffycloudg;
                int bb = m_CurrentColours->fluffycloudb;

                if (sundist < distLimit)
                {
                    hilight = (1.0f - fmax(*Foggyness, *CloudCoverage)) * (1.0f - sundist / (float)distLimit);
                    tr = tr * (1.0f - hilight) + 255 * hilight;
                    tg = tg * (1.0f - hilight) + 150 * hilight;
                    tb = tb * (1.0f - hilight) + 150 * hilight;
                    br = br * (1.0f - hilight) + 255 * hilight;
                    bg = bg * (1.0f - hilight) + 150 * hilight;
                    bb = bb * (1.0f - hilight) + 150 * hilight;

                    if (sundist < sundistBlocked) *SunBlockedByClouds = (fluffyalpha > (FLUFF_ALPHA / 2));
                }
                else
                {
                    hilight = 0.0f;
                }
                CloudOnScreen[i] = true;
                RenderBufferedOneXLUSprite_Rotate_2Colours(screenpos.x, screenpos.y, screenpos.z, szx * 55.0f, szy * 55.0f, tr, tg, tb, br, bg, bb, 0.0f, -1.0f,
                                                           1.0f / screenpos.z, (uint16_t)*IndividualRotation / 65336.0f * 6.28f + *ms_cameraRoll, fluffyalpha);
            }
            else
            {
                CloudOnScreen[i] = false;
            }
        }
        FlushSpriteBuffer();

        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, gpCloudTex[4]->raster);
        for(int i = 0; i < MAX_FLUFFY_CLOUDS; ++i)
        {
            if(!CloudOnScreen[i]) continue;

            RwV3d pos = { 2.0f * CoorsOffsetX[i], 2.0f * CoorsOffsetY[i], 40.0f * CoorsOffsetZ[i] + FLUFF_Z_OFFSET };
            worldpos.x = pos.x * rot_cos + pos.y * rot_sin + campos.x;
            worldpos.y = pos.x * rot_sin - pos.y * rot_cos + campos.y;
            worldpos.z = pos.z;

            if (CalcScreenCoors(worldpos, &screenpos, &szx, &szy, false, false))
            {
                if (sundist < sundistHilit)
                {
                    RenderBufferedOneXLUSprite_Rotate_Aspect(screenpos.x, screenpos.y, screenpos.z, szx * 30.0f, szy * 30.0f, 200 * hilight, 0, 0, 255, 1.0f / screenpos.z,
                                                             1.7f - GetATanOfXY(screenpos.x - *SunScreenX, screenpos.y - *SunScreenY) + *ms_cameraRoll, 255);
                }
            }
        }
        FlushSpriteBuffer();
    }
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Hooks
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
DECL_HOOK(bool, GameInit3, void* data)
{
    bool ret = GameInit3(data);
    FluffyCloudsTexDB = TextureDatabaseLoad("fluffyclouds", false, DF_Default);
    if(FluffyCloudsTexDB)
    {
        gpCloudTex[3] = GetTextureFromTexDB(FluffyCloudsTexDB, "cloudmasked");
        gpCloudTex[4] = GetTextureFromTexDB(FluffyCloudsTexDB, "cloudhilit");
    }
    return ret;
}
DECL_HOOKv(RenderEffects)
{
    if (CanSeeOutSideFromCurrArea()) RenderFluffyClouds();
    RenderEffects();
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
//////      Main
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
extern "C" void OnAllModsLoaded()
{
    logger->SetTag("FluffyCloudsSA");

    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    SET_TO(Foggyness, aml->GetSym(hGTASA, "_ZN8CWeather9FoggynessE"));
    SET_TO(ExtraSunnyness, aml->GetSym(hGTASA, "_ZN8CWeather14ExtraSunnynessE"));
    SET_TO(ms_cameraRoll, aml->GetSym(hGTASA, "_ZN7CClouds13ms_cameraRollE"));
    SET_TO(CloudRotation, aml->GetSym(hGTASA, "_ZN7CClouds13CloudRotationE"));
    SET_TO(SunScreenX, aml->GetSym(hGTASA, "_ZN8CCoronas10SunScreenXE"));
    SET_TO(SunScreenY, aml->GetSym(hGTASA, "_ZN8CCoronas10SunScreenYE"));
    SET_TO(CloudCoverage, aml->GetSym(hGTASA, "_ZN8CWeather13CloudCoverageE"));
    SET_TO(IndividualRotation, aml->GetSym(hGTASA, "_ZN7CClouds18IndividualRotationE"));
    SET_TO(TheCamera, aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(SunBlockedByClouds, aml->GetSym(hGTASA, "_ZN8CCoronas18SunBlockedByCloudsE"));
    SET_TO(m_CurrentColours, aml->GetSym(hGTASA, "_ZN10CTimeCycle16m_CurrentColoursE"));
    SET_TO(RsGlobal, aml->GetSym(hGTASA, "RsGlobal"));

    SET_TO(RwRenderStateSet, aml->GetSym(hGTASA, "_Z16RwRenderStateSet13RwRenderStatePv"));
    SET_TO(GetATanOfXY, aml->GetSym(hGTASA, "_ZN8CGeneral11GetATanOfXYEff"));
    SET_TO(CalcScreenCoors, aml->GetSym(hGTASA, "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb"));
    SET_TO(RenderBufferedOneXLUSprite_Rotate_2Colours, aml->GetSym(hGTASA, "_ZN7CSprite42RenderBufferedOneXLUSprite_Rotate_2ColoursEfffffhhhhhhffffh"));
    SET_TO(RenderBufferedOneXLUSprite_Rotate_Aspect, aml->GetSym(hGTASA, "_ZN7CSprite40RenderBufferedOneXLUSprite_Rotate_AspectEfffffhhhsffh"));
    SET_TO(FlushSpriteBuffer, aml->GetSym(hGTASA, "_ZN7CSprite17FlushSpriteBufferEv"));
    SET_TO(TextureDatabaseLoad, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime4LoadEPKcb21TextureDatabaseFormat"));
    SET_TO(GetEntry, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime8GetEntryEPKcRb"));
    SET_TO(GetRWTexture, aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime12GetRWTextureEi"));
    SET_TO(CanSeeOutSideFromCurrArea, aml->GetSym(hGTASA, "_ZN5CGame25CanSeeOutSideFromCurrAreaEv"));
    SET_TO(InitSpriteBuffer, aml->GetSym(hGTASA, "_ZN7CSprite16InitSpriteBufferEv"));

    HOOKPLT(GameInit3, pGTASA + BYVER(0x6742F0, 0x8470F0));
    HOOKPLT(RenderEffects, pGTASA + BYVER(0x670BD4, 0x8452B0));
}