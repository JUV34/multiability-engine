#include "global.h"
#include "test/battle.h"
#include "battle_ai_util.h"
#include "battle_controllers.h"
#include "battle_util.h"

static void ExpectPrimaryOnly(const struct BattlePokemon *mon, enum Ability ability)
{
    EXPECT_EQ(mon->ability, ability);
    EXPECT_EQ(mon->abilities[0], ability);
    for (u32 slot = 1; slot < MAX_BATTLER_ABILITIES; slot++)
        EXPECT_EQ(mon->abilities[slot], ABILITY_NONE);
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
        before = *player;
        gAiPartyData->mons[B_SIDE_PLAYER][gBattlerPartyIndexes[B_BATTLER_0]].ability = known;
        SaveBattlerData(B_BATTLER_0);
        SetBattlerData(B_BATTLER_0);
        EXPECT_EQ(player->ability, known);
        EXPECT_EQ(player->abilities[0], known);
        EXPECT_EQ(player->abilities[1], ABILITY_RIPEN);
        RestoreBattlerData(B_BATTLER_0);
        EXPECT_EQ(memcmp(player, &before, sizeof(before)), 0);
        allBefore = AllocSaveBattleMons();
        SetBattleMonAbility(player, ABILITY_NONE);
        player->abilities[1] = ABILITY_NONE;
        FreeRestoreBattleMons(allBefore);
        EXPECT_EQ(memcmp(player, &before, sizeof(before)), 0);
        player->abilities[1] = ABILITY_NONE;
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
