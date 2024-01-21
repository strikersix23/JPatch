// Boat and Skimmer on high FPS
uintptr_t Boat_ApplyWaterResistance_BackTo;
extern "C" float Boat_ApplyWaterResistance_Patch(float resistance)
{
    return resistance * GetTimeStepMagic();
}
__attribute__((optnone)) __attribute__((naked)) void Boat_ApplyWaterResistance_Inject(void)
{
    asm volatile(
        "VMOV R0, S14\n"
        "BL Boat_ApplyWaterResistance_Patch\n"
        "VMOV S14, R0\n"
        "VLDR S13, [R4, #0x18]\n"
        "ADD R5, R4, #0x74");
    asm volatile(
        "MOV PC, %0"
    :: "r" (Boat_ApplyWaterResistance_BackTo));
}

// Fix wheels rotation speed on high FPS
extern "C" float ProcessWheelRotation_Patch()
{
    return GetTimeStepMagic();
}
__attribute__((optnone)) __attribute__((naked)) void ProcessWheelRotation_Inject(void)
{
    asm volatile(
        "PUSH {LR}\n"
        "BL ProcessWheelRotation_Patch\n"
        "POP {LR}\n"
        "VMOV S14, R0\n");
    asm volatile(
        "VMUL.F32 S15, S15, S14\n"
        "VMOV R0, S15\n"
        "BX LR");
}

// Car Slowdown Fix
DECL_HOOKv(ProcessWheel_SlowDownFix, void *self, CVector &a1, CVector &a2, CVector &a3, CVector &a4, int a5, float a6, float a7, float a8, char a9, float *a10, void *a11, uint16_t a12)
{
    float& fWheelFriction = *(float*)(mod_HandlingManager + 4);
    float save = fWheelFriction; fWheelFriction *= GetTimeStepMagic();
    ProcessWheel_SlowDownFix(self, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
    fWheelFriction = save;
}
DECL_HOOKv(ProcessBikeWheel_SlowDownFix, void *self, CVector &a1, CVector &a2, CVector &a3, CVector &a4, int a5, float a6, float a7, float a8, float a9, char a10, float *a11, void *a12, int a13, uint16_t a14)
{
    float& fWheelFriction = *(float*)(mod_HandlingManager + 4);
    float save = fWheelFriction; fWheelFriction *= GetTimeStepMagic();
    ProcessBikeWheel_SlowDownFix(self, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
    fWheelFriction = save;
}

// Fix heli rotor rotation speed on high FPS
DECL_HOOKv(HeliRender_MatrixUpdate, void *self)
{
    HeliRender_MatrixUpdate(self);
    *fHeliRotorSpeed = 0.48307693f * GetTimeStepMagic();
}

// Fix pushing force
DECL_HOOKb(ApplyCollision_HighFPS, void *A, void *B, void *colpoint, float *impulseA, float *impulseB)
{
    *fl1679D4 = 0;//1.4f * GetTimeStepMagic();
    return ApplyCollision_HighFPS(A, B, colpoint, impulseA, impulseB);
}
DECL_HOOKv(ApplyCollision_MoveForce, void *self, CVector force)
{
    ApplyCollision_MoveForce(self, force * 0.0f);
}
DECL_HOOKv(ApplyCollision_TurnForce, void *self, CVector force, CVector center)
{
    ApplyCollision_TurnForce(self, force * 0.0f, center);
}
uintptr_t ApplyCollision_BackTo;
extern "C" float ApplyCollision_Patch(float invmass)
{
    return invmass * GetTimeStepMagic();
}
__attribute__((optnone)) __attribute__((naked)) void ApplyCollision_Inject(void)
{
    asm volatile(
        "VLDR S11, [R6, #0x10]\n"
        "VDIV.F32 S13, S7, S13\n"
        "VMOV R0, S13\n"
        "BL ApplyCollision_Patch\n"
        "VMOV S13, R0\n");
    asm volatile(
        "MOV PC, %0"
    :: "r" (ApplyCollision_BackTo));
}

// Streaming distance fix
float fStreamingDistFix;
uintptr_t CameraProcess_StreamDist_BackTo;
DECL_HOOKv(SetFOV_StreamingDistFix, float factor)
{
    // Someone is using broken mods
    // So here is the workaround + a little value clamping
    if(factor < 5.0f)
    {
        fStreamingDistFix = 70.0f / 5.0f;
    }
    else if(factor > 170.0f)
    {
        fStreamingDistFix = 70.0f / 170.0f;
    }
    else
    {
        fStreamingDistFix = 70.0f / factor;
    }

    fStreamingDistFix *= (*ms_fAspectRatio / ar43);

    SetFOV_StreamingDistFix(factor);
}
extern "C" void CameraProcess_StreamDist_Patch(int camera)
{
    *(float*)(camera + 0xF0) = fStreamingDistFix;
}
__attribute__((optnone)) __attribute__((naked)) void CameraProcess_StreamDist_Inject(void)
{
    asm volatile(
        "MOV R0, R4\n"
        "BL CameraProcess_StreamDist_Patch\n");
    asm volatile(
        "MOV PC, %0"
    :: "r" (CameraProcess_StreamDist_BackTo));
}

// fat-ass coronas!
DECL_HOOKb(CoronaSprite_CalcScreenCoors, CVector *worldPos, CVector *screenPos, float *scalex, float *scaley, bool a)
{
    bool ret = CoronaSprite_CalcScreenCoors(worldPos, screenPos, scalex, scaley, a);
    *scalex /= *ms_fAspectRatio;
    return ret;
}

// Vertex weight fix
uintptr_t VertexWeightFix_BackTo1, VertexWeightFix_BackTo2;
extern "C" uintptr_t VertexWeightFix_Patch(RpSkin *skin)
{
    return (skin->vertexMaps.maxWeights == 4) ? VertexWeightFix_BackTo2 : VertexWeightFix_BackTo1;
}
__attribute__((optnone)) __attribute__((naked)) void VertexWeightFix_Inject(void)
{
    asm volatile(
        "LDR R3, [R0, #0x18]\n"
        "PUSH {R1-R5}\n"
        "BL VertexWeightFix_Patch\n"
        "POP {R1-R5}\n"
        "MOV PC, R0");
}

// RE3: Road reflections
uintptr_t RoadReflections_BackTo;
__attribute__((optnone)) __attribute__((naked)) void RoadReflections_Inject(void)
{
    asm volatile(
        "VLDR S15, [R4, #0x18]\n"
        "LDR R2, [SP, #0x40]\n"
    );
    asm volatile(
        "MOV PC, %0"
    :: "r" (RoadReflections_BackTo));
}

// Fix traffic lights
DECL_HOOKv(TrFix_RenderEffects)
{
    BrightLightsRender();
    TrFix_RenderEffects();
}

// Framelimiter
static int curFPSLimit = 30;
static float frameMs = (1000.0f / 30.0f) * 1000.0f;
DECL_HOOKv(GameTick_RsEventHandler, int a1, void* a2)
{
    if(*m_PrefsFrameLimiter)
    {
        // Very bad framelimiter solution.
        // Does it need a pumpHack from GTA:SA?!
        OS_ThreadSleep((int)frameMs);
    }
    GameTick_RsEventHandler(a1, a2);
}

// Water UV Scroll
uintptr_t RenderWater_BackTo;
extern "C" float RenderWater_Patch(float windScale)
{
    return windScale * GetTimeStepMagic();
}
__attribute__((optnone)) __attribute__((naked)) void RenderWater_Inject(void)
{
    asm volatile(
        "VMOV R0, S18\n"
        "BL RenderWater_Patch\n"
        "VMOV S18, R0\n"

        "VCVT.F32.U32 S17, S14\n"
        "VMUL.F32 S30, S17, S30\n"
        "VADD.F32 S28, S18, S28\n");

    asm volatile(
        "MOV PC, %0"
    :: "r" (RenderWater_BackTo));
}

// Render States
static int fogState = 0;
CRGBA fogColor;
DECL_HOOKb(RwRenderState_Patch, RwRenderState state, int val)
{
    switch(state)
    {
        default:
            return RwRenderState_Patch(state, val);

        case rwRENDERSTATEFOGENABLE:
            if(fogState != val)
            {
                fogState = val;
                emu_DistanceFogSetEnabled(fogState != 0);
                if(val)
                {
                    emu_DistanceFogSetup(*m_fCurrentFogStart, 0.7f * *m_fCurrentFarClip, (float)fogColor.b / 255.0f, (float)fogColor.g / 255.0f, (float)fogColor.r / 255.0f);
                }
            }
            return true;

        case rwRENDERSTATEFOGCOLOR:
            fogColor.val = val;
            return true;

        // Shader does not support any other type of the fog...
        // Always rwFOGTYPELINEAR, no exponential (is this even used tho?)
        case rwRENDERSTATEFOGTYPE:
            return true;
    }
}

// Speedo cloudz
DECL_HOOKv(CloudsUpdate_Speedo)
{
    *fl1D4CF0 = 0.001f * GetTimeStepMagic();
    *fl1D4CF4 = 0.3f * GetTimeStepMagic();
    CloudsUpdate_Speedo();
}

// The Shadow. Of explosion. It's missing. Yup.
DECL_HOOKv(AddExplosion_AddShadow, uint8_t ShadowType, RwTexture *pTexture, CVector *pPosn,
                                   float fFrontX, float fFrontY, float fSideX, float fSideY,
                                   int16_t nIntensity, uint8_t nRed, uint8_t nGreen, uint8_t nBlue,
                                   float fZDistance, uint32_t nTime, float fScale)
{
    AddExplosion_AddShadow(ShadowType, pTexture, pPosn, 8.0f, 0.0f, 0.0f, -8.0f, 200, 0, 0, 0, 10.0f, 30000, 1.0f);
}

// Light shadow tweaked distance
DECL_HOOKv(StoreShadowForVehicle, uint32_t nId, uint8_t ShadowType, void *pTexture, CVector *pPosn, float fFrontX, float fFrontY, float fSideX, float fSideY, int16_t nIntensity, uint8_t nRed, uint8_t nGreen, uint8_t nBlue, float fZDistance, float fScale, float fDrawDistance, bool bTempShadow, float fUpDistance)
{
    StoreShadowForVehicle(nId, ShadowType, pTexture, pPosn, fFrontX, fFrontY, fSideX, fSideY, nIntensity, nRed, nGreen, nBlue, 15.0f, fScale, 120.0f, bTempShadow, 0.0f);
}

// Static shadows
DECL_HOOKv(InitShadows)
{
    static CPolyBunch bunchezTail[1024];
    
    InitShadows();
    for(int i = 0; i < 1023; ++i)
    {
        bunchezTail[i].m_pNext = &bunchezTail[i+1];
    }
    bunchezTail[1023].m_pNext = NULL;
    aPolyBunches[380-1].m_pNext = &bunchezTail[0]; // GTA:VC has 380 instead of 360 in SA?! LOL, DOWNGRADE
}

// Force DXT
DECL_HOOKp(LoadEntries_DXT, TextureDatabaseRuntime *self, bool a1, bool a2)
{
    self->loadedFormat = DF_DXT;
    return LoadEntries_DXT(self, a1, a2);
}