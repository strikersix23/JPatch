#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <dlfcn.h>
#include <time.h>

#include "GTASA_STRUCTS.h"

MYMODCFG(net.rusjj.jpatch, JPatch, 1.1.4, RusJJ)

union ScriptVariables
{
    int      i;
    float    f;
    uint32_t u;
    void*    p;
};

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;
static constexpr float fMagic = 50.0f / 30.0f;
float fEmergencyVehiclesFix;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CPlayerInfo* WorldPlayers;
CCamera* TheCamera;
RsGlobalType* RsGlobal;
MobileMenu *gMobileMenu;
CWidget** m_pWidgets;

float *ms_fTimeStep, *ms_fFOV, *game_FPS;
void *g_surfaceInfos;
unsigned int *m_snTimeInMilliseconds;
int *lastDevice, *NumberOfSearchLights;
bool *bDidWeProcessAnyCinemaCam, *bRunningCutscene;
ScriptVariables* ScriptParams;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void Redirect(uintptr_t addr, uintptr_t to)
{
    if(!addr) return;
    uint32_t hook[2] = {0xE51FF004, to};
    if(addr & 1)
    {
        addr &= ~1;
        if (addr & 2)
        {
            aml->PlaceNOP(addr, 1); 
            addr += 2;
        }
        hook[0] = 0xF000F8DF;
    }
    aml->Write(addr, (uintptr_t)hook, sizeof(hook));
}
void (*_rwOpenGLSetRenderState)(RwRenderState, int);
void (*_rwOpenGLGetRenderState)(RwRenderState, void*);
void (*ClearPedWeapons)(CPed*);
eBulletFxType (*GetBulletFx)(void* self, unsigned int surfaceId);
void (*LIB_PointerGetCoordinates)(int, int*, int*, float*);
bool (*Touch_IsDoubleTapped)(WidgetIDs, bool doTapEffect, int idkButBe1);
bool (*Touch_IsHeldDown)(WidgetIDs, int idkButBe1);
void (*SetCameraDirectlyBehindForFollowPed)(CCamera*);
void (*RestoreCamera)(CCamera*);
CVehicle* (*FindPlayerVehicle)(int playerId, bool unk);
void (*PhysicalApplyForce)(CPhysical* self, CVector force, CVector point, bool updateTurnSpeed);

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void adadad(void)
{
    asm("VMOV.F32 S0, #0.5");
}

// Moon phases
int moon_alphafunc, moon_vertexblend;
uintptr_t MoonVisual_1_BackTo;
extern "C" void MoonVisual_1(void)
{
    _rwOpenGLGetRenderState(rwRENDERSTATEALPHATESTFUNCTION, &moon_alphafunc);
    _rwOpenGLGetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, &moon_vertexblend);
    _rwOpenGLSetRenderState(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONALWAYS);
    _rwOpenGLSetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, true);

    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    _rwOpenGLSetRenderState(rwRENDERSTATEDESTBLEND, rwBLENDZERO);
}
__attribute__((optnone)) __attribute__((naked)) void MoonVisual_1_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl MoonVisual_1\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (MoonVisual_1_BackTo));
}
uintptr_t MoonVisual_2_BackTo;
extern "C" void MoonVisual_2(void)
{
    _rwOpenGLSetRenderState(rwRENDERSTATEALPHATESTFUNCTION, moon_alphafunc);
    _rwOpenGLSetRenderState(rwRENDERSTATEVERTEXALPHAENABLE, moon_vertexblend);
    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDONE);
    _rwOpenGLSetRenderState(rwRENDERSTATESRCBLEND, rwBLENDDESTALPHA);
    _rwOpenGLSetRenderState(rwRENDERSTATEZWRITEENABLE, false);
}
__attribute__((optnone)) __attribute__((naked)) void MoonVisual_2_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl MoonVisual_2\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (MoonVisual_2_BackTo));
}

// FOV
DECL_HOOKv(SetFOV, float factor, bool unused)
{
    if(TheCamera->m_bIsInCutscene)
    {
        *ms_fFOV = factor;
    }
    else
    {
        SetFOV(factor, unused);
    }
}

// Limit particles
uintptr_t AddBulletImpactFx_BackTo;
unsigned int nextHeavyParticleTick = 0;
eBulletFxType nLimitWithSparkles = BULLETFX_NOTHING;
extern "C" eBulletFxType AddBulletImpactFx(unsigned int surfaceId)
{
    eBulletFxType nParticlesType = GetBulletFx(g_surfaceInfos, surfaceId);
    if(nParticlesType == BULLETFX_SAND || nParticlesType == BULLETFX_DUST)
    {
        if(nextHeavyParticleTick < *m_snTimeInMilliseconds)
        {
            nextHeavyParticleTick = *m_snTimeInMilliseconds + 100;
        }
        else
        {
            return nLimitWithSparkles;
        }
    }
    return nParticlesType;
}
__attribute__((optnone)) __attribute__((naked)) void AddBulletImpactFx_inject(void)
{
    asm volatile(
        "mov r12, r3\n"
        "push {r0-r7,r9-r11}\n"
        "mov r0, r12\n"
        "bl AddBulletImpactFx\n"
        "mov r8, r0\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r7,r9-r11}\n"
        "mov r9, r1\n"
        "mov r4, r2\n"
        "bx r12\n"
    :: "r" (AddBulletImpactFx_BackTo));
}

// AimingRifleWalkFix
DECL_HOOKv(ControlGunMove, void* self, CVector2D* vec2D)
{
    float save = *ms_fTimeStep; *ms_fTimeStep = fMagic;
    ControlGunMove(self, vec2D);
    *ms_fTimeStep = save;
}

// SwimSpeedFix
uintptr_t SwimmingResistanceBack_BackTo;
float saveStep;
DECL_HOOKv(ProcessSwimmingResistance, void* self, CPed* ped)
{
    saveStep = *ms_fTimeStep;
    if(ped->m_nPedType == PED_TYPE_PLAYER1) *ms_fTimeStep *= 0.8954f/fMagic;
    else *ms_fTimeStep *= 1.14f/fMagic;
    ProcessSwimmingResistance(self, ped);
    *ms_fTimeStep = saveStep;
}
extern "C" void SwimmingResistanceBack(void)
{
    *ms_fTimeStep = saveStep;
}
__attribute__((optnone)) __attribute__((naked)) void SwimmingResistanceBack_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl SwimmingResistanceBack\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "vldr s4, [r0]\n"
        "ldr r0, [r4]\n"
        "vmul.f32 s0, s4, s0\n"
        "bx r12\n"
    :: "r" (SwimmingResistanceBack_BackTo));
}

// Madd Dogg's Mansion Basketball glitch
DECL_HOOK(ScriptHandle, GenerateNewPickup_MaddDogg, float x, float y, float z, int16_t modelId, ePickupType pickupType, int ammo, int16_t moneyPerDay, bool isEmpty, const char* msg)
{
    if(modelId == 1277 && x == 1263.05f && y == -773.67f && z == 1091.39f)
    {
        return GenerateNewPickup_MaddDogg(1291.2f, -798.0f, 1089.39f, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
    }
    return GenerateNewPickup_MaddDogg(x, y, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
}

// Fix Star bribe which is missing minus sign in X coordinate and spawns inside the building
DECL_HOOK(ScriptHandle, GenerateNewPickup_SFBribe, float x, float y, float z, int16_t modelId, ePickupType pickupType, int ammo, int16_t moneyPerDay, bool isEmpty, const char* msg)
{
    if(modelId == 1247 && (int)x == -2120 && (int)y == 96)
    {
        return GenerateNewPickup_SFBribe(2120.0f, y, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
    }
    return GenerateNewPickup_SFBribe(x, y, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
}

// Fix a Rifle weapon pickup that is located inside the stadium wall since beta
DECL_HOOK(ScriptHandle, GenerateNewPickup_SFRiflePickup, float x, float y, float z, int16_t modelId, ePickupType pickupType, int ammo, int16_t moneyPerDay, bool isEmpty, const char* msg)
{
    if(modelId == 357 && x == -2094.0f && y == -488.0f)
    {
        return GenerateNewPickup_SFRiflePickup(x, -490.2f, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
    }
    return GenerateNewPickup_SFRiflePickup(x, y, z, modelId, pickupType, ammo, moneyPerDay, isEmpty, msg);
}

// Do not drop-off jetpack in air
DECL_HOOKv(DropJetPackTask, void* task, CPed* ped)
{
    if(!ped->m_PedFlags.bIsStanding) return;
    DropJetPackTask(task, ped);
}

// Died penalty
uintptr_t DiedPenalty_BackTo;
extern "C" void DiedPenalty(void)
{
    if(WorldPlayers[0].m_nMoney > 0)
    {
        WorldPlayers[0].m_nMoney = WorldPlayers[0].m_nMoney - 100 < 0 ? 0 : WorldPlayers[0].m_nMoney - 100;
    }
    ClearPedWeapons(WorldPlayers[0].m_pPed);
}
__attribute__((optnone)) __attribute__((naked)) void DiedPenalty_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl DiedPenalty\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (DiedPenalty_BackTo));
}

// Emergency Vehicles
uintptr_t EmergencyVeh_BackTo;
__attribute__((optnone)) __attribute__((naked)) void EmergencyVeh_inject(void)
{
    asm volatile(
        "push {r0}\n");
    asm volatile(
        "vmov s0, %0\n"
    :: "r" (fEmergencyVehiclesFix));
    asm volatile(
        "mov r12, %0\n"
        "pop {r0}\n"
        "bx r12\n"
    :: "r" (EmergencyVeh_BackTo));
}
DECL_HOOKv(SetFOV_Emergency, float factor, bool unused)
{
    // Someone is using broken mods
    // So here is the workaround + a little value clamping
    if(factor < 20.0f)
    {
        fEmergencyVehiclesFix = 70.0f / 20.0f;
    }
    else if(factor > 160.0f)
    {
        fEmergencyVehiclesFix = 70.0f / 160.0f;
    }
    else
    {
        fEmergencyVehiclesFix = 70.0f / factor;
    }
    SetFOV_Emergency(factor, unused);
}

// Marker fix
DECL_HOOKv(PlaceRedMarker_MarkerFix, bool canPlace)
{
    if(canPlace)
    {
        int x, y;
        LIB_PointerGetCoordinates(*lastDevice, &x, &y, NULL);
        if(y > 0.85f * RsGlobal->maximumHeight &&
           x > ((float)RsGlobal->maximumWidth - 0.7f * RsGlobal->maximumHeight)) return;
    }
    PlaceRedMarker_MarkerFix(canPlace);
}

// SkimmerPlaneFix
// Changed the way it works, because ms_fTimeStep cannot be the same at the mod start (it is 0 at the mod start anyway)
uintptr_t SkimmerWaterResistance_BackTo;
extern "C" float SkimmerWaterResistance_patch(void)
{
    return 30.0f * (*ms_fTimeStep / fMagic);
}
__attribute__((optnone)) __attribute__((naked)) void SkimmerWaterResistance_inject(void)
{
    asm volatile(
        "vpush {s0-s2}\n"
        "bl SkimmerWaterResistance_patch\n"
        "vpop {s0-s2}\n"
        "vmov.f32 s4, r0\n");
    asm volatile(
        "mov r12, %0\n"
        "vadd.f32 s2, s2, s8\n"
        "bx r12\n"
    :: "r" (SkimmerWaterResistance_BackTo));
}

// Cinematic camera
bool toggledCinematic = false;
DECL_HOOKv(PlayerInfoProcess_Cinematic, CPlayerInfo* info, int playerNum)
{
    PlayerInfoProcess_Cinematic(info, playerNum);

    // Do it for local player only.
    if(info == &WorldPlayers[0])
    {
        if(!*bRunningCutscene &&
           info->m_pPed->m_pVehicle != NULL &&
           info->m_pPed->m_nPedState == PEDSTATE_DRIVING)
        {
            if(info->m_pPed->m_pVehicle->m_nVehicleType != VEHICLE_TYPE_TRAIN &&
               Touch_IsDoubleTapped(WIDGETID_CAMERAMODE, true, 1))
            {
                toggledCinematic = !TheCamera->m_bEnabledCinematicCamera;
                TheCamera->m_bEnabledCinematicCamera = toggledCinematic;

                memset(m_pWidgets[WIDGETID_CAMERAMODE]->tapTimes, 0, sizeof(float)*10); // CWidget::ClearTapHistory in a better way
            }
        }
        else
        {
            if(toggledCinematic)
            {
                TheCamera->m_bEnabledCinematicCamera = false;
                *bDidWeProcessAnyCinemaCam = false;
                if(!*bRunningCutscene &&
                   TheCamera->m_pTargetEntity == NULL)
                {
                    RestoreCamera(TheCamera);
                    SetCameraDirectlyBehindForFollowPed(TheCamera);
                }
                m_pWidgets[WIDGETID_CAMERAMODE]->enabled = false;
                toggledCinematic = false;
            }
        }
    }
}

// SWAT
uintptr_t GetCarGunFired_BackTo; // For our usage only
uintptr_t GetCarGunFired_BackTo1, GetCarGunFired_BackTo2; // For optimization?
extern "C" void GetCarGunFired_patch(void)
{
    CVehicle* veh = FindPlayerVehicle(-1, false);
    if(veh != NULL && (veh->m_nModelIndex == 407 || veh->m_nModelIndex == 601))
    {
        GetCarGunFired_BackTo = GetCarGunFired_BackTo1;
    }
    else
    {
        GetCarGunFired_BackTo = GetCarGunFired_BackTo2;
    }
}
__attribute__((optnone)) __attribute__((naked)) void GetCarGunFired_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl GetCarGunFired_patch\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (GetCarGunFired_BackTo));
}

// Fuzzy seek (im lazy to patch so lets just do this instead (because we need to inject the code))
DECL_HOOK(int, mpg123_param, void* mh, int key, long val, int ZERO, double fval)
{
    // 0x2000 = MPG123_SKIP_ID3V2
    // 0x200  = MPG123_FUZZY
    // 0x100  = MPG123_SEEKBUFFER
    // 0x40   = MPG123_GAPLESS
    return mpg123_param(mh, key, val | (0x2000 | 0x200 | 0x100 | 0x40), ZERO, fval);
}

// Fix water cannon
DECL_HOOKv(WaterCannonRender, void* self)
{
    float save = *ms_fTimeStep; *ms_fTimeStep = fMagic;
    WaterCannonRender(self);
    *ms_fTimeStep = save;
}
DECL_HOOKv(WaterCannonUpdate, void* self, int frames)
{
    float save = *ms_fTimeStep; *ms_fTimeStep = fMagic;
    WaterCannonUpdate(self, frames);
    *ms_fTimeStep = save;
}

// Moving objs (opcode 034E)
uintptr_t ProcessCommands800To899_BackTo;
extern "C" void ProcessCommands800To899_patch(void)
{
    float scale = 30.0f / *game_FPS;
    ScriptParams[4].f *= scale;
    ScriptParams[5].f *= scale;
    ScriptParams[6].f *= scale;
}
__attribute__((optnone)) __attribute__((naked)) void ProcessCommands800To899_inject(void)
{
    asm volatile(
        "push {r0-r11}\n"
        "bl ProcessCommands800To899_patch\n");
    asm volatile(
        "mov r12, %0\n"
        "pop {r0-r11}\n"
        "bx r12\n"
    :: "r" (ProcessCommands800To899_BackTo));
}

// PhysicalApplyCollision
uintptr_t PhysicalApplyCollision_BackTo;
extern "C" void PhysicalApplyCollision_patch(CPhysical* self, CVector force, CVector point, bool updateTurnSpeed)
{
    force *= *ms_fTimeStep / fMagic;
    PhysicalApplyForce(self, force, point, updateTurnSpeed);
}
__attribute__((optnone)) __attribute__((naked)) void PhysicalApplyCollision_inject(void)
{
    asm volatile(
        "mov r0, r9\n"
        "ldr.w r11, [sp,#0xE0+0xAC]\n"
        "bl PhysicalApplyCollision_patch\n");
    asm volatile(
        "mov r0, %0\n"
        "bx r0\n"
    :: "r" (PhysicalApplyCollision_BackTo));
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void OnModLoad()
{
    logger->SetTag("JPatch");
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = dlopen("libGTASA.so", RTLD_LAZY);

    // Functions Start //
    SET_TO(_rwOpenGLSetRenderState, aml->GetSym(hGTASA, "_Z23_rwOpenGLSetRenderState13RwRenderStatePv"));
    SET_TO(_rwOpenGLGetRenderState, aml->GetSym(hGTASA, "_Z23_rwOpenGLGetRenderState13RwRenderStatePv"));
    SET_TO(ClearPedWeapons,         aml->GetSym(hGTASA, "_ZN4CPed12ClearWeaponsEv"));
    SET_TO(GetBulletFx,             aml->GetSym(hGTASA, "_ZN14SurfaceInfos_c11GetBulletFxEj"));
    SET_TO(LIB_PointerGetCoordinates, aml->GetSym(hGTASA, "_Z25LIB_PointerGetCoordinatesiPiS_Pf"));
    SET_TO(Touch_IsDoubleTapped,    aml->GetSym(hGTASA, "_ZN15CTouchInterface14IsDoubleTappedENS_9WidgetIDsEbi"));
    SET_TO(Touch_IsHeldDown,        aml->GetSym(hGTASA, "_ZN15CTouchInterface10IsHeldDownENS_9WidgetIDsEi"));
    SET_TO(SetCameraDirectlyBehindForFollowPed, aml->GetSym(hGTASA, "_ZN7CCamera48SetCameraDirectlyBehindForFollowPed_CamOnAStringEv"));
    SET_TO(RestoreCamera,           aml->GetSym(hGTASA, "_ZN7CCamera7RestoreEv"));
    SET_TO(FindPlayerVehicle,       aml->GetSym(hGTASA, "_Z17FindPlayerVehicleib"));
    SET_TO(PhysicalApplyForce,      aml->GetSym(hGTASA, "_ZN9CPhysical10ApplyForceE7CVectorS0_b"));
    // Functions End   //
    
    // Variables Start //
    SET_TO(ms_fTimeStep,            aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(WorldPlayers,            *(void**)(pGTASA + 0x6783C8)); // Patched CWorld::Players will work now!
    SET_TO(ms_fFOV,                 aml->GetSym(hGTASA, "_ZN5CDraw7ms_fFOVE"));
    SET_TO(game_FPS,                aml->GetSym(hGTASA, "_ZN6CTimer8game_FPSE"));
    SET_TO(TheCamera,               aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(RsGlobal,                aml->GetSym(hGTASA, "RsGlobal"));
    SET_TO(g_surfaceInfos,          aml->GetSym(hGTASA, "g_surfaceInfos"));
    SET_TO(m_snTimeInMilliseconds,  aml->GetSym(hGTASA, "_ZN6CTimer22m_snTimeInMillisecondsE"));
    SET_TO(gMobileMenu,             aml->GetSym(hGTASA, "gMobileMenu"));
    SET_TO(NumberOfSearchLights,    aml->GetSym(hGTASA, "_ZN5CHeli20NumberOfSearchLightsE"));
    SET_TO(lastDevice,              aml->GetSym(hGTASA, "lastDevice"));
    SET_TO(m_pWidgets,              *(void**)(pGTASA + 0x67947C)); // Patched CTouchInterface::m_pWidgets will work now!
    SET_TO(bDidWeProcessAnyCinemaCam, aml->GetSym(hGTASA, "bDidWeProcessAnyCinemaCam"));
    SET_TO(bRunningCutscene,        aml->GetSym(hGTASA, "_ZN12CCutsceneMgr10ms_runningE"));
    SET_TO(ScriptParams,            *(void**)(pGTASA + 0x676F7C)); // Patched ScriptParams will work now!
    // Variables End   //

    // Animated textures
    if(cfg->Bind("EnableAnimatedTextures", true, "Visual")->GetBool())
    {
        aml->Write(aml->GetSym(hGTASA, "RunUVAnim"), (uintptr_t)"\x01", 1);
    }

    // Vertex weight
    if(cfg->Bind("FixVertexWeight", true, "Visual")->GetBool())
    {
        aml->Write(pGTASA + 0x1C8064, (uintptr_t)"\x01", 1);
        aml->Write(pGTASA + 0x1C8082, (uintptr_t)"\x01", 1);
    }

    // Fix moon! (lack of rendering features)
    //if(cfg->Bind("MoonPhases", true, "Visual")->GetBool())
    //{
    //    MoonVisual_1_BackTo = pGTASA + 0x59ED90 + 0x1;
    //    MoonVisual_2_BackTo = pGTASA + 0x59EE4E + 0x1;
    //    Redirect(pGTASA + 0x59ED80 + 0x1, (uintptr_t)MoonVisual_1_inject);
    //    Redirect(pGTASA + 0x59EE36 + 0x1, (uintptr_t)MoonVisual_2_inject);
    //}

    // Fix corona's on wet roads
    //if(cfg->Bind("FixCoronasReflectionOnWetRoads", true, "Visual")->GetBool())
    //{
    //    // Nothing
    //}

    // Fix sky multitude
    if(cfg->Bind("FixSkyMultitude", true, "Visual")->GetBool())
    {
        aml->Unprot(pGTASA + 0x59FB8C, 2*sizeof(float));
        *(float*)(pGTASA + 0x59FB8C) = -10.0f;
        *(float*)(pGTASA + 0x59FB90) =  10.0f;
    }

    // Fix vehicles backlights light state
    if(cfg->Bind("FixCarsBacklightLightState", true, "Visual")->GetBool())
    {
        aml->Write(pGTASA + 0x591272, (uintptr_t)"\x02", 1);
        aml->Write(pGTASA + 0x59128E, (uintptr_t)"\x02", 1);
    }

    // Limit sand/dust particles on bullet impact (they are EXTREMELY dropping FPS)
    if(cfg->Bind("LimitSandDustBulletParticles", true, "Visual")->GetBool())
    {
        AddBulletImpactFx_BackTo = pGTASA + 0x36478E + 0x1;
        Redirect(pGTASA + 0x36477C + 0x1, (uintptr_t)AddBulletImpactFx_inject);
        if(cfg->Bind("LimitSandDustBulletParticlesWithSparkles", false, "Visual")->GetBool())
        {
            nLimitWithSparkles = BULLETFX_SPARK;
        }
    }

    // Do not set primary color to the white on vehicles paintjob
    if(cfg->Bind("PaintJobDontSetPrimaryToWhite", true, "Visual")->GetBool())
    {
        aml->PlaceNOP(pGTASA + 0x582328, 2);
    }

    // Fix walking while rifle-aiming
    if(cfg->Bind("FixAimingWalkRifle", true, "Gameplay")->GetBool())
    {
        HOOKPLT(ControlGunMove, pGTASA + 0x66F9D0);
    }

    // Fix slow swimming speed
    if(cfg->Bind("SwimmingSpeedFix", true, "Gameplay")->GetBool())
    {
        SwimmingResistanceBack_BackTo = pGTASA + 0x53BD3A + 0x1;
        HOOKPLT(ProcessSwimmingResistance, pGTASA + 0x66E584);
        Redirect(pGTASA + 0x53BD30 + 0x1, (uintptr_t)SwimmingResistanceBack_inject);
    }

    // Fix stealable items sucking
    if(cfg->Bind("ClampObjectToStealDist", true, "Gameplay")->GetBool())
    {
        aml->Write(pGTASA + 0x40B162, (uintptr_t)"\xB7\xEE\x00\x0A", 4);
    }

    // Fix broken basketball minigame by placing the save icon away from it
    if(cfg->Bind("MaddDoggMansionSaveFix", true, "Gameplay")->GetBool())
    {
        HOOKPLT(GenerateNewPickup_MaddDogg, pGTASA + 0x674DE4);
    }

    // Fix broken basketball minigame by placing the save icon away from it
    if(cfg->Bind("FixStarBribeInSFBuilding", true, "Gameplay")->GetBool())
    {
        HOOKPLT(GenerateNewPickup_SFBribe, pGTASA + 0x674DE4);
    }

    // Fix rifle pickup that stuck inside the stadium
    if(cfg->Bind("FixSFStadiumRiflePickup", true, "Gameplay")->GetBool())
    {
        HOOKPLT(GenerateNewPickup_SFRiflePickup, pGTASA + 0x674DE4);
    }

    // Remove jetpack leaving on widget press while in air?
    if(cfg->Bind("DisableDropJetPackInAir", true, "Gameplay")->GetBool())
    {
        HOOKPLT(DropJetPackTask, pGTASA + 0x675AA8);
    }

    // Dont stop the car before leaving it
    if(cfg->Bind("ImmediatelyLeaveTheCar", true, "Gameplay")->GetBool())
    {
        aml->PlaceNOP(pGTASA + 0x409A18, 3);
    }

    // Bring back penalty when CJ dies!
    if(cfg->Bind("WeaponPenaltyIfDied", true, "Gameplay")->GetBool())
    {
        DiedPenalty_BackTo = pGTASA + 0x3088E0 + 0x1;
        Redirect(pGTASA + 0x3088BE + 0x1, (uintptr_t)DiedPenalty_inject);
    }

    // Fix emergency vehicles
    if(cfg->Bind("FixEmergencyVehicles", true, "Gameplay")->GetBool())
    {
        EmergencyVeh_BackTo = pGTASA + 0x3DD88C + 0x1;
        Redirect(pGTASA + 0x3DD87A + 0x1, (uintptr_t)EmergencyVeh_inject);
        HOOKPLT(SetFOV_Emergency, pGTASA + 0x673DDC);
    }

    // Fix cutscene FOV (disabled by default right now, causes the camera being too close on ultrawide screens)
    if(cfg->Bind("FixCutsceneFOV", false, "Visual")->GetBool())
    {
        HOOKPLT(SetFOV, pGTASA + 0x673DDC);
    }

    // Fix red marker that cannot be placed in a menu on ultrawide screens
    // Kinda trashy fix...
    if(cfg->Bind("FixRedMarkerUnplaceable", true, "Gameplay")->GetBool())
    {
        aml->Unprot(pGTASA + 0x2A9E60, sizeof(float));
        *(float*)(pGTASA + 0x2A9E60) /= 1.2f;
        aml->Write(pGTASA + 0x2A9D42, (uintptr_t)"\x83\xEE\x0C\x3A", 4);
        HOOKPLT(PlaceRedMarker_MarkerFix, pGTASA + 0x6702C8);
    }

    // Dont set player on fire when he's on burning BMX (MTA:SA)
    if(cfg->Bind("DontBurnPlayerOnBurningBMX", true, "Gameplay")->GetBool())
    {
        Redirect(pGTASA + 0x3F1ECC + 0x1, pGTASA + 0x3F1F24 + 0x1);
    }

    // Increase the number of vehicles types (not actual vehicles) that can be loaded at once (MTA:SA)
    // Causes crash and completely useless
    //if(cfg->Bind("DesiredNumOfCarsLoadedBuff", true, "Gameplay")->GetBool())
    //{
    //    *(unsigned char*)(aml->GetSym(hGTASA, "_ZN10CStreaming24desiredNumVehiclesLoadedE")) = 50; // Game hardcoded to 50 max (lazy to fix crashes for patches below)
    //    aml->PlaceNOP(pGTASA + 0x46BE1E, 1);
    //    aml->PlaceNOP(pGTASA + 0x47269C, 2);
    //}

    // THROWN projectiles throw more accurately (MTA:SA)
    if(cfg->Bind("ThrownProjectilesAccuracy", true, "Gameplay")->GetBool())
    {
        Redirect(pGTASA + 0x5DBBC8 + 0x1, pGTASA + 0x5DBD0C + 0x1);
    }

    // Disable call to FxSystem_c::GetCompositeMatrix in CAEFireAudioEntity::UpdateParameters 
    // that was causing a crash - spent ages debugging, the crash happens if you create 40 or 
    // so vehicles that catch fire (upside down) then delete them, repeating a few times.
    // (MTA:SA)
    if(cfg->Bind("GetCompositeMatrixFixPossibleCrash", true, "Gameplay")->GetBool())
    {
        aml->PlaceNOP(pGTASA + 0x395E6A, 7);
    }

    // Disable setting the occupied's vehicles health to 75.0f when a burning ped enters it (MTA:SA)
    if(cfg->Bind("DontGiveCarHealthFromBurningPed", true, "Gameplay")->GetBool())
    {
        aml->PlaceNOP(pGTASA + 0x3F1CAC, 0xD);
    }

    // Increase intensity of vehicle tail light corona (MTA:SA)
    // Is this even working on Android?
    if(cfg->Bind("IncreaseTailLightIntensity", true, "Gameplay")->GetBool())
    {
        aml->Write(pGTASA + 0x591016, (uintptr_t)"\xF0", 1);
    }

    // Cinematic vehicle camera on double tap
    if(cfg->Bind("CinematicCameraOnDoubleTap", true, "Gameplay")->GetBool())
    {
        HOOKPLT(PlayerInfoProcess_Cinematic, pGTASA + 0x673E84);
    }
    
    // Fix Skimmer plane ( https://github.com/XMDS )
    if (cfg->Bind("SkimmerPlaneFix", true, "Gameplay")->GetBool())
    {
        SkimmerWaterResistance_BackTo = pGTASA + 0x589ADC + 0x1;
        Redirect(pGTASA + 0x589AD4 + 0x1, (uintptr_t)SkimmerWaterResistance_inject);
    }

    // Buff streaming
    if(cfg->Bind("BuffStreamingMem", true, "Gameplay")->GetBool())
    {
        aml->PlaceNOP(pGTASA + 0x46BE18, 1);
        aml->PlaceNOP(pGTASA + 0x47272A, 2);
        aml->PlaceNOP(pGTASA + 0x472690, 2);
        int* streamingAvailable = (int*)aml->GetSym(hGTASA, "_ZN10CStreaming18ms_memoryAvailableE");
        if(*streamingAvailable <= 50*1024*1024)
        {
            *streamingAvailable = 512 * 1024 * 1024;
        }
    }

    // Buff planes max height
    if(cfg->Bind("BuffPlanesMaxHeight", true, "Gameplay")->GetBool())
    {
        float* heights;
        aml->Unprot(pGTASA + 0x585674, sizeof(float)*7);
        SET_TO(heights, pGTASA + 0x585674);
        for(int i = 0; i < 7; ++i)
        {
            heights[i] *= 1.25f;
        }
    }

    // Buff jetpack max height
    if(cfg->Bind("BuffJetpackMaxHeight", true, "Gameplay")->GetBool())
    {
        aml->Unprot(pGTASA + 0x5319D0, sizeof(float));
        *(float*)(pGTASA + 0x5319D0) *= 2.0f;
    }

    // 44100 Hz Audio support (without a mod OpenAL Update) (is this working?)
    if(cfg->Bind("Allow44100HzAudio", true, "Gameplay")->GetBool())
    {
        aml->Unprot(pGTASA + 0x613E0A, sizeof(int));
        *(int*)(pGTASA + 0x613E0A) = 44100;
    }

    // Disable GTA vehicle detachment at rotation awkwardness
    if(cfg->Bind("FixVehicleDetachmentAtRot", true, "Visual")->GetBool())
    {
        Redirect(pGTASA + 0x407344 + 0x1, pGTASA + 0x407016 + 0x1);
    }

    // Bring back missing "Shoot" button for S.W.A.T. when we dont have a weapon. WarDrum forgot about it.
    if(cfg->Bind("FixMissingShootBtnForSWAT", true, "Gameplay")->GetBool())
    {
        GetCarGunFired_BackTo1 = pGTASA + 0x3F99E8 + 0x1;
        GetCarGunFired_BackTo2 = pGTASA + 0x3F9908 + 0x1;
        Redirect(pGTASA + 0x3F99C4 + 0x1, (uintptr_t)GetCarGunFired_inject);
    }

    // Just a fuzzy seek. Tell MPG123 to not load useless data.
    if(cfg->Bind("FuzzySeek", true, "Gameplay")->GetBool())
    {
        HOOKPLT(mpg123_param, pGTASA + 0x66F3D4);
    }

    // Fix water cannon on a high fps
    if(cfg->Bind("FixHighFPSWaterCannons", true, "Gameplay")->GetBool())
    {
        HOOKPLT(WaterCannonRender, pGTASA + 0x67432C);
        HOOKPLT(WaterCannonUpdate, pGTASA + 0x6702EC);
    }

    // Fix moving objects on a high fps (through the scripts)
    if(cfg->Bind("FixHighFPSOpcode034E", true, "Gameplay")->GetBool())
    {
        ProcessCommands800To899_BackTo = pGTASA + 0x347866 + 0x1;
        Redirect(pGTASA + 0x346E88 + 0x1, (uintptr_t)ProcessCommands800To899_inject);
    }

    // Fix pushing force
    if(cfg->Bind("FixPhysicalPushForce", true, "Gameplay")->GetBool())
    {
        PhysicalApplyCollision_BackTo = pGTASA + 0x402B72 + 0x1;
        Redirect(pGTASA + 0x402B68 + 0x1, (uintptr_t)PhysicalApplyCollision_inject);
    }

    // Can now rotate the camera inside the heli/plane?
    // https://github.com/TheOfficialFloW/gtasa_vita/blob/6417775e182b0c8b789cc9a0c1161e6f1b43814f/loader/main.c#L736
    if(cfg->Bind("UnstuckHeliCamera", true, "Gameplay")->GetBool())
    {
        aml->Write(pGTASA + 0x3C0866, (uintptr_t)"\x00\x20\x00\xBF", 4);
        aml->Write(pGTASA + 0x3C1518, (uintptr_t)"\x00\x20\x00\xBF", 4);
        aml->Write(pGTASA + 0x3C198A, (uintptr_t)"\x00\x20\x00\xBF", 4);
        aml->Write(pGTASA + 0x3FC462, (uintptr_t)"\x00\x20\x00\xBF", 4);
        aml->Write(pGTASA + 0x3FC754, (uintptr_t)"\x00\x20\x00\xBF", 4);
    }

    // Fix those freakin small widgets!
    //if(cfg->Bind("FixWidgetsSizeDropping", true, "Gameplay")->GetBool())
    //{
    //    // Nothing
    //}
}