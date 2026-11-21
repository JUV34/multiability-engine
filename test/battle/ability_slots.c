#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_ai_switch.h"
#include "battle_controllers.h"
#include "battle_util.h"

static void ExpectPrimaryOnly(const struct BattlePokemon *mon, enum Ability ability)
{
    EXPECT_EQ(mon->ability, ability);
    EXPECT_EQ(mon->abilities[0], ability);
    for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
        EXPECT_EQ(mon->abilities[slot], ABILITY_NONE);
}

SINGLE_BATTLE_TEST("MS2 raw and active queries find four abilities without changing primary attribution")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        enum Ability previous = gLastUsedAbility;
        const enum Ability abilities[MAX_BATTLER_ABILITIES] = {ABILITY_BLAZE, ABILITY_RIPEN, ABILITY_SOUNDPROOF, ABILITY_WATER_ABSORB};
        for (u32 slot = 0; slot < MAX_BATTLER_ABILITIES; slot++)
        {
            EXPECT(SetBattleMonAbilityAt(player, slot, abilities[slot]));
            EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, slot), abilities[slot]);
            EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, slot), abilities[slot]);
            EXPECT(BattlerHasAbility(B_BATTLER_0, abilities[slot]));
        }
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITY_TORRENT));
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITY_NONE));
        EXPECT_EQ(GetBattlerAbility(B_BATTLER_0), ABILITY_BLAZE);
        EXPECT_EQ(GetBattlerAbilityIgnoreMoldBreaker(B_BATTLER_0), ABILITY_BLAZE);
        EXPECT_EQ(GetBattlerAbilityNoAbilityShield(B_BATTLER_0), ABILITY_BLAZE);
        EXPECT_EQ(gLastUsedAbility, previous);
        // Existing primary-only writers must not discard the extras.
        SetBattleMonAbility(player, ABILITY_TORRENT);
        EXPECT_EQ(player->abilities[0], ABILITY_TORRENT);
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, 1), ABILITY_RIPEN);
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITY_BLAZE));
    }
}

SINGLE_BATTLE_TEST("MS2 raw queries are canonical bounded and independent of field presence")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        // Deliberately poison the mirror to verify raw slot 0's canonical input.
        player->abilities[0] = ABILITY_TORRENT;
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, 0), ABILITY_BLAZE);
        SetBattleMonAbility(player, ABILITY_BLAZE);
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, MAX_BATTLER_ABILITIES), ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, (u32)-1), ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityRawAt((enum BattlerId)-1, 0), ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt((enum BattlerId)-1, 0), ABILITY_NONE);
        EXPECT(!BattlerHasAbility((enum BattlerId)-1, ABILITY_NONE));
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITIES_COUNT));
        player->abilities[1] = ABILITIES_COUNT;
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, 1), ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_NONE);
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_RIPEN));
        gBattleStruct->battlerState[B_BATTLER_0].notOnField = TRUE;
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, 1), ABILITY_RIPEN);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_NONE);
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITY_RIPEN));
        gBattleStruct->battlerState[B_BATTLER_0].notOnField = FALSE;
        gSpecialStatuses[B_BATTLER_0].attackerInParty = TRUE;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_NONE);
        gSpecialStatuses[B_BATTLER_0].attackerInParty = FALSE;
        player->hp = 0;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_RIPEN);
    }
}

SINGLE_BATTLE_TEST("MS2 empty and duplicate slots retain legacy NONE query behavior")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        EXPECT_EQ(BattlerHasAbility(B_BATTLER_0, ABILITY_NONE), GetBattlerAbility(B_BATTLER_0) == ABILITY_NONE);
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_BLAZE));
        EXPECT(BattlerHasAbility(B_BATTLER_0, ABILITY_BLAZE));
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_NONE));
        EXPECT(BattlerHasAbility(B_BATTLER_0, ABILITY_BLAZE));
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_RIPEN));
        SetBattleMonAbility(player, ABILITY_NONE);
        EXPECT(BattlerHasAbility(B_BATTLER_0, ABILITY_NONE));
        EXPECT(BattlerHasAbility(B_BATTLER_0, ABILITY_RIPEN));
    }
}

SINGLE_BATTLE_TEST("MS2 Gastro Acid uses each candidate's suppressibility regardless of primary or Shield")
{
    enum Ability primary = ABILITY_NONE;
    enum Item item = ITEM_NONE;
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_MULTITYPE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_ABILITY_SHIELD; }
    PARAMETRIZE { primary = ABILITY_MULTITYPE; item = ITEM_ABILITY_SHIELD; }
    GIVEN {
        PLAYER(SPECIES_CHARMANDER) { Item(item); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        SetBattleMonAbility(player, primary);
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_LEVITATE));
        EXPECT(SetBattleMonAbilityAt(player, 2, ABILITY_MULTITYPE));
        player->volatiles.gastroAcid = TRUE;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 0), GetBattlerAbility(B_BATTLER_0));
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 2), ABILITY_MULTITYPE);
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, 1), ABILITY_LEVITATE);
        player->volatiles.gastroAcid = FALSE;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_LEVITATE);
    }
}

SINGLE_BATTLE_TEST("MS2 Neutralizing Gas uses candidate metadata and Shield without recursive queries")
{
    enum Ability primary = ABILITY_NONE;
    enum Item item = ITEM_NONE;
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_MULTITYPE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_ABILITY_SHIELD; }
    PARAMETRIZE { primary = ABILITY_MULTITYPE; item = ITEM_ABILITY_SHIELD; }
    GIVEN {
        PLAYER(SPECIES_CHARMANDER) { Item(item); }
        OPPONENT(SPECIES_KOFFING) { Ability(ABILITY_NEUTRALIZING_GAS); }
    } WHEN {
        TURN {}
    } THEN {
        EXPECT(IsNeutralizingGasOnField());
        SetBattleMonAbility(player, primary);
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_LEVITATE));
        EXPECT(SetBattleMonAbilityAt(player, 2, ABILITY_MULTITYPE));
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 0), GetBattlerAbility(B_BATTLER_0));
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), item == ITEM_ABILITY_SHIELD ? ABILITY_LEVITATE : ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 2), ABILITY_MULTITYPE);
        EXPECT_EQ(GetBattlerAbilityNoAbilityShield(B_BATTLER_0), primary == ABILITY_MULTITYPE ? primary : ABILITY_NONE);
        opponent->volatiles.neutralizingGas = FALSE;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_LEVITATE);
    }
}

SINGLE_BATTLE_TEST("MS2 Mold Breaker checks the candidate rather than the primary and preserves bypass variants")
{
    enum Ability primary = ABILITY_NONE;
    enum Item item = ITEM_NONE;
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_LEVITATE; item = ITEM_NONE; }
    PARAMETRIZE { primary = ABILITY_BLAZE; item = ITEM_ABILITY_SHIELD; }
    PARAMETRIZE { primary = ABILITY_LEVITATE; item = ITEM_ABILITY_SHIELD; }
    GIVEN {
        PLAYER(SPECIES_CHARMANDER) { Item(item); }
        OPPONENT(SPECIES_PINSIR) { Ability(ABILITY_MOLD_BREAKER); }
    } WHEN {
        TURN {}
    } THEN {
        SetBattleMonAbility(player, primary);
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_LEVITATE));
        EXPECT(SetBattleMonAbilityAt(player, 2, ABILITY_BLAZE));
        gBattlerAttacker = B_BATTLER_1;
        gBattleStruct->moldBreakerActive = TRUE;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 0), GetBattlerAbility(B_BATTLER_0));
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), item == ITEM_ABILITY_SHIELD ? ABILITY_LEVITATE : ABILITY_NONE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 2), ABILITY_BLAZE);
        EXPECT_EQ(GetBattlerAbilityIgnoreMoldBreaker(B_BATTLER_0), primary);
        EXPECT_EQ(GetBattlerAbilityNoAbilityShield(B_BATTLER_0), primary == ABILITY_LEVITATE ? ABILITY_NONE : primary);
        gBattlerAttacker = B_BATTLER_0;
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 1), ABILITY_LEVITATE);
    }
}

SINGLE_BATTLE_TEST("MS2 transformed Gastro Acid suppresses Comatose in every slot")
{
    u32 slot = 0;
    bool32 transformed = FALSE, gastroAcid = FALSE;
    for (u32 s = 0; s < MAX_BATTLER_ABILITIES; s++)
        for (u32 t = 0; t < 2; t++)
            for (u32 g = 0; g < 2; g++)
                PARAMETRIZE { slot = s; transformed = t; gastroAcid = g; }
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        EXPECT(SetBattleMonAbilityAt(player, slot, ABILITY_COMATOSE));
        player->volatiles.transformed = transformed;
        player->volatiles.gastroAcid = gastroAcid;
        EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, slot), ABILITY_COMATOSE);
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, slot), transformed && gastroAcid ? ABILITY_NONE : ABILITY_COMATOSE);
        EXPECT_EQ(BattlerHasAbility(B_BATTLER_0, ABILITY_COMATOSE), !(transformed && gastroAcid));
        EXPECT_EQ(GetBattlerAbilityAt(B_BATTLER_0, 0), GetBattlerAbility(B_BATTLER_0));
    }
}

AI_SINGLE_BATTLE_TEST("MS2 nested AI scopes hide unknown extras preserve own extras and restore both sides")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER) { Moves(MOVE_TACKLE, MOVE_GROWL); }
        OPPONENT(SPECIES_WOBBUFFET) { Moves(MOVE_CELEBRATE); }
    } WHEN {
        TURN { EXPECT_MOVE(opponent, MOVE_CELEBRATE); }
    } THEN {
        struct AiExtraAbilitySnapshot outer, inner;
        EXPECT(!BattlerHasAi(B_BATTLER_0));
        EXPECT(BattlerHasAi(B_BATTLER_1));
        EXPECT(SetBattleMonAbilityAt(player, 1, ABILITY_GORILLA_TACTICS));
        EXPECT(SetBattleMonAbilityAt(player, 2, ABILITY_SOUNDPROOF));
        EXPECT(SetBattleMonAbilityAt(player, 3, ABILITY_WATER_ABSORB));
        EXPECT(SetBattleMonAbilityAt(opponent, 1, ABILITY_RIPEN));
        AI_SaveAndMaskExtraAbilities(&outer);
        EXPECT(!BattlerHasAbility(B_BATTLER_0, ABILITY_GORILLA_TACTICS));
        for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
            EXPECT_EQ(GetBattlerAbilityRawAt(B_BATTLER_0, slot), ABILITY_NONE);
        EXPECT(BattlerHasAbility(B_BATTLER_1, ABILITY_RIPEN));
        AI_SaveAndMaskExtraAbilities(&inner);
        AI_RestoreExtraAbilities(&inner);
        EXPECT_EQ(player->abilities[1], ABILITY_NONE);
        AI_RestoreExtraAbilities(&outer);
        EXPECT(BattlerHasAbility(B_BATTLER_0, ABILITY_GORILLA_TACTICS));
        EXPECT_EQ(player->abilities[2], ABILITY_SOUNDPROOF);
        EXPECT_EQ(player->abilities[3], ABILITY_WATER_ABSORB);
        EXPECT_EQ(opponent->abilities[1], ABILITY_RIPEN);

        gBattleStruct->choicedMove[B_BATTLER_0] = MOVE_TACKLE;
        EXPECT(CheckMoveLimitations(B_BATTLER_0, 0, MOVE_LIMITATION_CHOICE_ITEM) & (1u << 1));
        SetBattlerAiData(B_BATTLER_0, gAiLogicData);
        EXPECT(!(gAiLogicData->moveLimitations[B_BATTLER_0] & (1u << 1)));
        EXPECT_EQ(player->abilities[1], ABILITY_GORILLA_TACTICS);
        gBattleStruct->monToSwitchIntoId[B_BATTLER_1] = 0;
        EXPECT_EQ(GetMostSuitableMonToSwitchInto(B_BATTLER_1, SWITCH_AFTER_KO), 0);
        EXPECT_EQ(player->abilities[1], ABILITY_GORILLA_TACTICS);
    }
}

SINGLE_BATTLE_TEST("MS1-R controller packets contain natural or hidden primary and empty extras")
{
    enum Ability ability = ABILITY_NONE;
    PARAMETRIZE { ability = ABILITY_SYNCHRONIZE; }
    PARAMETRIZE { ability = ABILITY_TELEPATHY; }
    GIVEN {
        PLAYER(SPECIES_RALTS) { Ability(ability); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        struct BattlePokemon packet;
        void (*oldEnd)(enum BattlerId) = gBattlerControllerEndFuncs[B_BATTLER_0];
        enum Ability previous = gLastUsedAbility;
        ExpectPrimaryOnly(player, ability);
        gBattlerControllerEndFuncs[B_BATTLER_0] = BattleControllerDummy;
        BtlController_EmitGetMonData(B_BATTLER_0, B_COMM_TO_CONTROLLER, REQUEST_ALL_BATTLE, 0);
        BtlController_HandleGetMonData(B_BATTLER_0);
        gBattlerControllerEndFuncs[B_BATTLER_0] = oldEnd;
        EXPECT_EQ(gBattleResources->bufferB[B_BATTLER_0][2]
                | (gBattleResources->bufferB[B_BATTLER_0][3] << 8), sizeof(packet));
        EXPECT_LE(sizeof(packet) + 4, sizeof(gBattleResources->transferBuffer));
        memcpy(&packet, &gBattleResources->bufferB[B_BATTLER_0][4], sizeof(packet));
        ExpectPrimaryOnly(&packet, ability);
        EXPECT_EQ(gLastUsedAbility, previous);
        // Candidate/form producers must also discard stale extra slots.
        player->abilities[1] = ABILITY_RIPEN;
        CopyMonAbilityAndTypesToBattleMon(B_BATTLER_0, GetBattlerMon(B_BATTLER_0));
        ExpectPrimaryOnly(player, ability);
    }
}

SINGLE_BATTLE_TEST("MS1-R switch-in restores the natural primary and initializes extras")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { SWITCH(player, 1); }
    } THEN {
        EXPECT_EQ(player->species, SPECIES_CHARMANDER);
        ExpectPrimaryOnly(player, ABILITY_BLAZE);
    }
}

SINGLE_BATTLE_TEST("MS1-R unforced Mega Evolution and reversion preserve primary derivation")
{
    GIVEN {
        PLAYER(SPECIES_VENUSAUR) { Item(ITEM_VENUSAURITE); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_CELEBRATE, gimmick: GIMMICK_MEGA); }
    } THEN {
        EXPECT_EQ(player->species, SPECIES_VENUSAUR_MEGA);
        ExpectPrimaryOnly(player, ABILITY_THICK_FAT);
        // The runner restores the party form before THEN, not the battle snapshot.
        EXPECT_EQ(GetMonData(GetBattlerMon(B_BATTLER_0), MON_DATA_SPECIES), SPECIES_VENUSAUR);
        EXPECT_EQ(GetMonAbility(GetBattlerMon(B_BATTLER_0)), ABILITY_OVERGROW);
    }
}

SINGLE_BATTLE_TEST("MS1-R unforced weather form changes and reverts during battle")
{
    GIVEN {
        PLAYER(SPECIES_CASTFORM_NORMAL);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_SUNNY_DAY); }
        TURN { MOVE(player, MOVE_SANDSTORM); }
    } SCENE {
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, player);
        ANIMATION(ANIM_TYPE_GENERAL, B_ANIM_FORM_CHANGE, player);
    } THEN {
        EXPECT_EQ(player->species, SPECIES_CASTFORM_NORMAL);
        ExpectPrimaryOnly(player, ABILITY_FORECAST);
    }
}

SINGLE_BATTLE_TEST("MS1-R AI masking and restore preserve primary and injected extras")
{
    enum Ability known = ABILITY_NONE;
    PARAMETRIZE { known = ABILITY_NONE; }
    PARAMETRIZE { known = ABILITY_SYNCHRONIZE; }
    GIVEN {
        PLAYER(SPECIES_RALTS) { Ability(ABILITY_TELEPATHY); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
    } THEN {
        struct BattlePokemon before;
        struct BattlePokemon *allBefore;
        ExpectPrimaryOnly(player, ABILITY_TELEPATHY);
        EXPECT(!BattlerHasAi(B_BATTLER_0));
        player->abilities[1] = ABILITY_RIPEN;
        player->abilities[2] = ABILITY_SOUNDPROOF;
        player->abilities[3] = ABILITY_WATER_ABSORB;
        before = *player;
        gAiPartyData->mons[B_SIDE_PLAYER][gBattlerPartyIndexes[B_BATTLER_0]].ability = known;
        SaveBattlerData(B_BATTLER_0);
        SetBattlerData(B_BATTLER_0);
        EXPECT_EQ(player->ability, known);
        EXPECT_EQ(player->abilities[0], known);
        for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
            EXPECT_EQ(player->abilities[slot], ABILITY_NONE);
        RestoreBattlerData(B_BATTLER_0);
        EXPECT_EQ(memcmp(player, &before, sizeof(before)), 0);
        allBefore = AllocSaveBattleMons();
        SetBattleMonAbility(player, ABILITY_NONE);
        for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
            player->abilities[slot] = ABILITY_NONE;
        FreeRestoreBattleMons(allBefore);
        EXPECT_EQ(memcmp(player, &before, sizeof(before)), 0);
        for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
            player->abilities[slot] = ABILITY_NONE;
    }
}

SINGLE_BATTLE_TEST("MS1-R Skill Swap synchronizes both primary mirrors")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_SQUIRTLE);
    } WHEN {
        TURN { MOVE(player, MOVE_SKILL_SWAP); }
    } THEN {
        ExpectPrimaryOnly(player, ABILITY_TORRENT);
        ExpectPrimaryOnly(opponent, ABILITY_BLAZE);
    }
}

SINGLE_BATTLE_TEST("MS1-R Trace synchronizes the copied primary")
{
    GIVEN {
        PLAYER(SPECIES_RALTS) { Ability(ABILITY_TRACE); }
        OPPONENT(SPECIES_CHARMANDER);
    } WHEN {
        TURN {}
    } THEN {
        ExpectPrimaryOnly(player, ABILITY_BLAZE);
    }
}

SINGLE_BATTLE_TEST("MS1-R Mummy synchronizes the contact replacement")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_YAMASK);
    } WHEN {
        TURN { MOVE(player, MOVE_AQUA_JET); }
    } THEN {
        ExpectPrimaryOnly(player, ABILITY_MUMMY);
    }
}

SINGLE_BATTLE_TEST("MS1-R Wandering Spirit synchronizes both contact swap mirrors")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_RUNERIGUS);
    } WHEN {
        TURN { MOVE(player, MOVE_AQUA_JET); }
    } THEN {
        ExpectPrimaryOnly(player, ABILITY_WANDERING_SPIRIT);
        ExpectPrimaryOnly(opponent, ABILITY_BLAZE);
    }
}

DOUBLE_BATTLE_TEST("MS1-R Receiver and Power of Alchemy synchronize inherited primary")
{
    enum Species species = SPECIES_NONE;
    enum Ability ability = ABILITY_NONE;
    PARAMETRIZE { species = SPECIES_PASSIMIAN; ability = ABILITY_RECEIVER; }
    PARAMETRIZE { species = SPECIES_MUK_ALOLA; ability = ABILITY_POWER_OF_ALCHEMY; }
    GIVEN {
        PLAYER(species) { Ability(ability); }
        PLAYER(SPECIES_CHARMANDER) { HP(1); }
        OPPONENT(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(opponentLeft, MOVE_SCRATCH, target: playerRight); }
    } THEN {
        ExpectPrimaryOnly(playerLeft, ABILITY_BLAZE);
    }
}

SINGLE_BATTLE_TEST("MS1-R Entrainment synchronizes the overwritten primary")
{
    GIVEN {
        PLAYER(SPECIES_CHARMANDER);
        OPPONENT(SPECIES_SQUIRTLE);
    } WHEN {
        TURN { MOVE(player, MOVE_ENTRAINMENT); }
    } THEN {
        ExpectPrimaryOnly(opponent, ABILITY_BLAZE);
    }
}

SINGLE_BATTLE_TEST("MS1-R Transform copies primary and mirror together")
{
    GIVEN {
        PLAYER(SPECIES_DITTO) { Ability(ABILITY_LIMBER); }
        OPPONENT(SPECIES_CHARMANDER);
    } WHEN {
        TURN { MOVE(player, MOVE_TRANSFORM); }
    } THEN {
        EXPECT(player->volatiles.transformed);
        EXPECT_EQ(player->species, SPECIES_CHARMANDER);
        ExpectPrimaryOnly(player, ABILITY_BLAZE);
    }
}
